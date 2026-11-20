#!/usr/bin/env node
/**
 * ua-arch-layers.js — Phase 2 semantic layer assignment (deterministic classifier).
 * Reads Phase 1 results + script input, assigns every file node to exactly one layer,
 * writes layers.json and validates the assignment.
 */
'use strict';
const fs = require('fs');

const ROOT = '/home/zcli/dev/Parametric-Feature-Based-Modeling/表达式与重算';
const input = JSON.parse(fs.readFileSync(`${ROOT}/.ua/tmp/ua-arch-input.json`, 'utf8'));
const results = JSON.parse(fs.readFileSync(`${ROOT}/.ua/tmp/ua-arch-results.json`, 'utf8'));

// Layer order = dependency flow, high layers first (each layer depends only on layers below it).
const LAYERS = [
  {
    id: 'layer:entry-demo',
    name: '演示入口层',
    description: '每周的 main.cpp：把该周的 lexer→parser→求值/依赖图/重算模块组装成可运行流水线，以控制台演示呈现当周课程目标与验收场景。',
  },
  {
    id: 'layer:dependency-recompute',
    name: '依赖图与增量重算层',
    description: 'month02 的核心模块 dependencies 与 engine：从公式抽取属性依赖、用拓扑排序确定计算顺序并检测循环依赖，engine 协调脏标记传播实现增量重算与错误恢复。',
  },
  {
    id: 'layer:formula-binding',
    name: '公式绑定与持久化层',
    description: 'month01/week04 引入的 formulas 与 storage 模块：把表达式绑定到属性通道形成公式引擎，并负责参数化矩形模型连同公式的保存与加载。',
  },
  {
    id: 'layer:evaluator',
    name: '表达式求值层',
    description: 'evaluator.cpp/.cppm 遍历 AST 求出表达式数值并统一错误处理，month01/week03 引入后成为公式求值与 month02 增量重算的执行核心。',
  },
  {
    id: 'layer:domain-model',
    name: '属性与领域模型层',
    description: 'property、rectangle 与 change_signal 模块：以事务和观察者机制维护属性通道、变更通知与参数化矩形案例的派生属性，是表达式绑定的领域载体。',
  },
  {
    id: 'layer:parser',
    name: '语法分析与 AST 层',
    description: 'parser.cpp/.cppm 以递归下降法把 Token 流解析为 AST，month01/week02 引入并被后续各周原样复用，供求值器与依赖抽取共同消费。',
  },
  {
    id: 'layer:lexer',
    name: '词法分析层',
    description: '各周自带的 lexer.cpp/.cppm：把表达式文本切分为数字、标识符与运算符 Token，是整个表达式引擎流水线的最底层入口。',
  },
  {
    id: 'layer:build-tooling',
    name: '构建与工具链配置层',
    description: '每周自包含的 CMakeLists.txt、CMakePresets.json、run.sh、.clangd 与 .clang-format，加上 .ua 分析工具配置：支撑 Clang 22 + CMake 4.3 import std 的一键构建与 IDE 体验。',
  },
  {
    id: 'layer:documentation',
    name: '课程文档层',
    description: '根 README 课程导航、每周课程讲义 README 与 tools/README 构建说明：覆盖五问答案、练习与验收标准，并把各周串成 month01→month02 的学习路径。',
  },
];

// Assignment rules — first match wins. Order matters.
const RULES = [
  [/^document$/, () => 'layer:documentation'],
  [/./, n => {
    const name = n.name;
    if (name === 'main.cpp') return 'layer:entry-demo';
    if (/^lexer\./.test(name)) return 'layer:lexer';
    if (/^parser\./.test(name)) return 'layer:parser';
    if (/^evaluator\./.test(name)) return 'layer:evaluator';
    if (/^(property|rectangle|change_signal)\./.test(name)) return 'layer:domain-model';
    if (/^(formulas|storage)\./.test(name)) return 'layer:formula-binding';
    if (/^(dependencies|engine)\./.test(name)) return 'layer:dependency-recompute';
    return 'layer:build-tooling'; // CMakeLists.txt, CMakePresets.json, run.sh, .clangd, .clang-format, .ua/*
  }],
];

const byId = new Map(input.fileNodes.map(n => [n.id, n]));
const buckets = new Map(LAYERS.map(l => [l.id, []]));
const unassigned = [];

for (const node of input.fileNodes) {
  let target = null;
  for (const [re, fn] of RULES) {
    if (re.test(node.type)) { target = fn(node); break; }
  }
  if (!target || !buckets.has(target)) { unassigned.push(node.id); continue; }
  buckets.get(target).push(node.id);
}

if (unassigned.length) {
  console.error('FATAL: unassigned nodes:', unassigned);
  process.exit(1);
}

// ---- validation ----
const total = input.fileNodes.length;
const layerIds = new Set(LAYERS.map(l => l.id));
const assigned = [];
for (const l of LAYERS) assigned.push(...buckets.get(l.id));
if (assigned.length !== total) {
  console.error(`FATAL: assigned ${assigned.length} != total ${total}`);
  process.exit(1);
}
const seen = new Set();
for (const id of assigned) {
  if (seen.has(id)) { console.error('FATAL: duplicate assignment', id); process.exit(1); }
  seen.add(id);
}
if (seen.size !== byId.size) { console.error('FATAL: count mismatch vs input nodes'); process.exit(1); }

const output = LAYERS.map(l => ({
  id: l.id,
  name: l.name,
  description: l.description,
  nodeIds: buckets.get(l.id),
}));

fs.mkdirSync(`${ROOT}/.ua/intermediate`, { recursive: true });
fs.writeFileSync(`${ROOT}/.ua/intermediate/layers.json`, JSON.stringify(output, null, 2));

console.log(`OK: ${LAYERS.length} layers, ${total} file nodes assigned, written to .ua/intermediate/layers.json`);
output.forEach(l => console.log(`  ${l.id.padEnd(28)} ${l.name.padEnd(12)} ${String(l.nodeIds.length).padStart(3)} files`));
// cross-check against Phase 1 total
console.log(`Phase1 totalFileNodes = ${results.fileStats.totalFileNodes}; sum(nodeIds) = ${output.reduce((s, l) => s + l.nodeIds.length, 0)}`);

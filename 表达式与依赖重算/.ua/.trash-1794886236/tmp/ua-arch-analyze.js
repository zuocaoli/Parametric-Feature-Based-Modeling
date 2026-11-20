#!/usr/bin/env node
/**
 * ua-arch-analyze.js — Phase 1 structural analysis for architecture layering.
 * Usage: node ua-arch-analyze.js <input.json> <output.json>
 * Input : { fileNodes:[{id,type,name,filePath,summary,tags}], importEdges:[{source,target,type}], allEdges:[...] }
 * Output: structural pattern report (see README of the architecture-analyzer skill).
 * Exit 0 on success, 1 on fatal error.
 */
'use strict';

const fs = require('fs');

function main() {
  const [, , inPath, outPath] = process.argv;
  if (!inPath || !outPath) {
    console.error('usage: node ua-arch-analyze.js <input.json> <output.json>');
    process.exit(1);
  }
  const input = JSON.parse(fs.readFileSync(inPath, 'utf8'));
  const fileNodes = input.fileNodes || [];
  const importEdges = input.importEdges || [];
  const allEdges = input.allEdges || [];
  if (!Array.isArray(fileNodes) || fileNodes.length === 0) throw new Error('no file nodes in input');

  const byId = new Map(fileNodes.map(n => [n.id, n]));
  const pathOf = id => (byId.get(id) && byId.get(id).filePath) || id;
  const typeOf = id => (byId.get(id) && byId.get(id).type) || 'file';

  // ---------- A. Directory grouping (top-level, after common prefix) ----------
  const dirOf = p => {
    const i = p.lastIndexOf('/');
    return i === -1 ? '(root)' : p.slice(0, i);
  };
  // common directory prefix across all files
  const dirParts = fileNodes.map(n => dirOf(n.filePath).split('/').filter(Boolean));
  let prefix = [];
  if (dirParts.length) {
    const first = dirParts[0];
    for (let i = 0; i < first.length; i++) {
      const seg = first[i];
      if (dirParts.every(parts => parts[i] === seg)) prefix.push(seg);
      else break;
    }
    // a common prefix only counts if it is a real directory shared by ALL files
    if (dirParts.some(parts => parts.length === prefix.length && parts.length === dirOfPartsLength(parts, prefix))) {
      // prefix equals the whole directory of some file -> those files sit in the prefix dir itself
    }
  }
  function dirOfPartsLength(parts, pre) { return pre.length; }

  const prefixStr = prefix.length ? prefix.join('/') + '/' : '';
  const groupOf = p => {
    let rel = p;
    if (prefixStr && rel.startsWith(prefixStr)) rel = rel.slice(prefixStr.length);
    return dirOf(rel);
  };

  const directoryGroups = {};
  for (const n of fileNodes) {
    const g = groupOf(n.filePath);
    (directoryGroups[g] = directoryGroups[g] || []).push(n.id);
  }

  // ---------- A2 (extra). Week-level grouping (monthNN/weekYY) — this repo's real units ----------
  const WEEK_RE = /^(month\d+)\/(week\d+)\//;
  const weekGroups = {};
  for (const n of fileNodes) {
    const m = WEEK_RE.exec(n.filePath);
    const g = m ? `${m[1]}/${m[2]}` : groupOf(n.filePath);
    (weekGroups[g] = weekGroups[g] || []).push(n.id);
  }

  // ---------- B. Node type grouping ----------
  const nodeTypeGroups = {};
  for (const n of fileNodes) (nodeTypeGroups[n.type] = nodeTypeGroups[n.type] || []).push(n.id);

  // ---------- C. Adjacency: fan-in / fan-out ----------
  const fanOut = {}, fanIn = {};
  for (const e of importEdges) {
    if (!byId.has(e.source) || !byId.has(e.target)) continue;
    fanOut[e.source] = (fanOut[e.source] || 0) + 1;
    fanIn[e.target] = (fanIn[e.target] || 0) + 1;
  }

  const groupEdges = (edges, keyFn) => {
    const out = {};
    for (const e of edges) {
      const a = keyFn(e.source), b = keyFn(e.target);
      if (a == null || b == null) continue;
      const key = a === b ? null : `${a}|${b}`;
      if (key) out[key] = (out[key] || 0) + 1;
    }
    return out;
  };

  // ---------- E. Inter-group import frequency (importEdges) ----------
  const interGroup = groupEdges(importEdges, id => groupOf(pathOf(id)));
  const interGroupImports = Object.entries(interGroup)
    .map(([k, c]) => { const [from, to] = k.split('|'); return { from, to, count: c }; })
    .sort((x, y) => y.count - x.count);

  const interWeek = groupEdges(importEdges, id => {
    const m = WEEK_RE.exec(pathOf(id));
    return m ? `${m[1]}/${m[2]}` : groupOf(pathOf(id));
  });
  const interWeekImports = Object.entries(interWeek)
    .map(([k, c]) => { const [from, to] = k.split('|'); return { from, to, count: c }; })
    .sort((x, y) => y.count - x.count);

  // ---------- F. Intra-group import density ----------
  function density(groups, edges) {
    const res = {};
    const groupById = new Map();
    for (const [g, ids] of Object.entries(groups)) for (const id of ids) groupById.set(id, g);
    const total = {}, internal = {};
    for (const g of Object.keys(groups)) { total[g] = 0; internal[g] = 0; }
    for (const e of edges) {
      if (!groupById.has(e.source) && !groupById.has(e.target)) continue;
      const ga = groupById.get(e.source), gb = groupById.get(e.target);
      for (const g of new Set([ga, gb].filter(Boolean))) total[g]++;
      if (ga && ga === gb) { internal[ga]++; total[ga]++; }
    }
    for (const g of Object.keys(groups)) {
      res[g] = { internalEdges: internal[g], totalEdges: total[g], density: total[g] ? +(internal[g] / total[g]).toFixed(3) : 0 };
    }
    return res;
  }
  const intraGroupDensity = density(directoryGroups, importEdges);
  const intraWeekDensity = density(weekGroups, importEdges);

  // ---------- D. Cross-category dependency analysis (allEdges) ----------
  const crossMap = {};
  for (const e of allEdges) {
    const ft = typeOf(e.source), tt = typeOf(e.target);
    const key = `${ft}|${tt}|${e.type}`;
    crossMap[key] = (crossMap[key] || 0) + 1;
  }
  const crossCategoryEdges = Object.entries(crossMap)
    .map(([k, count]) => { const [fromType, toType, edgeType] = k.split('|'); return { fromType, toType, edgeType, count }; })
    .sort((x, y) => y.count - x.count);

  // ---------- G. Directory pattern matching ----------
  const DIR_PATTERNS = [
    [/^(routes|api|controllers|endpoints|handlers|serializers|routers|controller|blueprints)$/, 'api'],
    [/^(services|core|lib|domain|logic|signals|composables|mailers|jobs|channels|internal)$/, 'service'],
    [/^(models|db|data|persistence|repository|entities|migrations|entity|sql|database|schema)$/, 'data'],
    [/^(components|views|pages|ui|layouts|screens)$/, 'ui'],
    [/^(middleware|plugins|interceptors|guards)$/, 'middleware'],
    [/^(utils|helpers|common|shared|tools|templatetags|pkg)$/, 'utility'],
    [/^(config|constants|env|settings|management|commands)$/, 'config'],
    [/^(__tests__|test|tests|spec|specs)$/, 'test'],
    [/^(types|interfaces|schemas|contracts|dtos|dto|request|response)$/, 'types'],
    [/^hooks$/, 'hooks'],
    [/^(store|state|reducers|actions|slices)$/, 'state'],
    [/^(assets|static|public)$/, 'assets'],
    [/^cmd$|^bin$/, 'entry'],
    [/^(docs|documentation|wiki)$/, 'documentation'],
    [/^(deploy|deployment|infra|infrastructure|k8s|kubernetes|helm|charts|terraform|tf|docker)$/, 'infrastructure'],
    [/^(\.github|\.gitlab|\.circleci)$/, 'ci-cd'],
  ];
  const matchDir = name => {
    for (const [re, label] of DIR_PATTERNS) if (re.test(name)) return label;
    return null;
  };
  const patternMatches = {};
  for (const g of Object.keys(directoryGroups)) patternMatches[g] = matchDir(g);

  // file-level patterns (extra, week-role oriented for this repo)
  const FILE_PATTERNS = [
    [/^lexer\./, 'lexer'],
    [/^parser\./, 'parser'],
    [/^evaluator\./, 'evaluator'],
    [/^property\./, 'property-system'],
    [/^rectangle\./, 'domain-model'],
    [/^change_signal\./, 'observer-signal'],
    [/^formulas\./, 'formula-binding'],
    [/^storage\./, 'persistence'],
    [/^dependencies\./, 'dependency-graph'],
    [/^engine\./, 'recompute-engine'],
    [/^main\.cpp$/, 'entry-point'],
    [/^CMakeLists\.txt$/, 'build-cmake'],
    [/^CMakePresets\.json$/, 'build-presets'],
    [/^run\.sh$/, 'build-script'],
    [/^\.clangd$/, 'ide-config'],
    [/^\.clang-format$/, 'format-config'],
    [/^README\.md$/, 'documentation'],
  ];
  const filePatterns = {};
  for (const n of fileNodes) {
    let label = null;
    for (const [re, l] of FILE_PATTERNS) if (re.test(n.name)) { label = l; break; }
    if (label) filePatterns[n.id] = label;
  }
  // role census per week: which pipeline roles each week contains
  const weekRoles = {};
  for (const [wk, ids] of Object.entries(weekGroups)) {
    weekRoles[wk] = {};
    for (const id of ids) {
      const r = filePatterns[id] || 'other';
      weekRoles[wk][r] = (weekRoles[wk][r] || 0) + 1;
    }
  }

  // ---------- H. Deployment topology ----------
  const allPaths = fileNodes.map(n => n.filePath);
  const infraFiles = allPaths.filter(p =>
    /Dockerfile/.test(p) || /docker-compose\./.test(p) ||
    /(\.tf|\.tfvars)$/.test(p) || /(^|\/)(k8s|kubernetes|helm|charts|deploy|docker)\//.test(p) ||
    /\.github\/workflows\//.test(p) || /\.gitlab-ci\.yml$/.test(p) || /Jenkinsfile$/.test(p) || /Makefile$/.test(p)
  );
  const buildFiles = allPaths.filter(p => /CMakeLists\.txt$|CMakePresets\.json$|(^|\/)run\.sh$/.test(p));
  const deploymentTopology = {
    hasDockerfile: infraFiles.some(p => /Dockerfile/.test(p)),
    hasCompose: infraFiles.some(p => /docker-compose\./.test(p)),
    hasK8s: infraFiles.some(p => /(k8s|kubernetes|helm|charts)\//.test(p)),
    hasTerraform: infraFiles.some(p => /\.tf(vars)?$/.test(p)),
    hasCI: infraFiles.some(p => /\.github\/workflows\//.test(p) || /\.gitlab-ci\.yml$/.test(p) || /Jenkinsfile$/.test(p)),
    infraFiles,
    buildFiles,
    hasBuildScripts: buildFiles.length > 0,
    notes: '纯本地 C++ 课程仓库：无 Docker/K8s/Terraform/CI，部署面仅剩每周一键构建脚本 run.sh 与 CMake 预设。',
  };

  // ---------- I. Data pipeline detection ----------
  const pick = re => allPaths.filter(p => re.test(p));
  const dataPipeline = {
    schemaFiles: pick(/\.(sql|graphql|gql|proto|prisma)$/),
    migrationFiles: pick(/migrations\//),
    dataModelFiles: pick(/(^|\/)(property|rectangle)\.(cpp|cppm)$/),
    apiHandlerFiles: [],
    entryFiles: pick(/(^|\/)main\.cpp$/),
    expressionPipeline: {
      lexer: pick(/(^|\/)lexer\.(cpp|cppm)$/).length,
      parser: pick(/(^|\/)parser\.(cpp|cppm)$/).length,
      evaluator: pick(/(^|\/)evaluator\.(cpp|cppm)$/).length,
      formulaBinding: pick(/(^|\/)formulas\.(cpp|cppm)$/).length,
      dependencyGraph: pick(/(^|\/)dependencies\.(cpp|cppm)$/).length,
      recomputeEngine: pick(/(^|\/)engine\.(cpp|cppm)$/).length,
    },
    notes: '数据流是表达式流水线而非数据库管线：lexer → parser(AST) → evaluator → formulas 绑定 → dependencies 依赖图 → engine 增量重算；property/rectangle 是被驱动的领域模型。',
  };

  // ---------- J. Documentation coverage ----------
  const docRe = /README\.md$|(^|\/)docs\//;
  const groupsWithDocsList = [];
  const undocumentedGroups = [];
  for (const [g, ids] of Object.entries(directoryGroups)) {
    const has = ids.some(id => docRe.test(pathOf(id)));
    (has ? groupsWithDocsList : undocumentedGroups).push(g);
  }
  const totalGroups = Object.keys(directoryGroups).length;
  const docCoverage = {
    groupsWithDocs: groupsWithDocsList.length,
    totalGroups,
    coverageRatio: totalGroups ? +(groupsWithDocsList.length / totalGroups).toFixed(2) : 0,
    undocumentedGroups,
    groupsWithDocs: groupsWithDocsList.length,
    documentedGroupNames: groupsWithDocsList,
    docFiles: allPaths.filter(p => /\.md$/.test(p)),
  };

  // ---------- K. Dependency direction ----------
  const pairCounts = {};
  for (const e of importEdges) {
    if (!byId.has(e.source) || !byId.has(e.target)) continue;
    const a = groupOf(pathOf(e.source)), b = groupOf(pathOf(e.target));
    if (a === b) continue;
    const key = [a, b].sort().join('~');
    pairCounts[key] = pairCounts[key] || {};
    pairCounts[key][`${a}->${b}`] = (pairCounts[key][`${a}->${b}`] || 0) + 1;
  }
  const dependencyDirection = [];
  for (const [key, dirs] of Object.entries(pairCounts)) {
    const entries = Object.entries(dirs).sort((x, y) => y[1] - x[1]);
    const [best, c] = entries[0];
    const [dependent, dependsOn] = best.split('->');
    dependencyDirection.push({ dependent, dependsOn, count: c });
  }
  dependencyDirection.sort((x, y) => y.count - x.count);

  // ---------- Extra: similar_to clusters (deliberate cross-week copies) ----------
  const similarEdges = allEdges.filter(e => e.type === 'similar_to');
  const similarByWeek = {};
  for (const e of similarEdges) {
    const wa = (WEEK_RE.exec(pathOf(e.source)) || [])[0] || groupOf(pathOf(e.source));
    const wb = (WEEK_RE.exec(pathOf(e.target)) || [])[0] || groupOf(pathOf(e.target));
    const key = wa === wb ? wa : `${wa}~${wb}`;
    similarByWeek[key] = (similarByWeek[key] || 0) + 1;
  }

  // ---------- Extra: documents/configures targets per group ----------
  const documentsTargets = {};
  for (const e of allEdges) {
    if (e.type !== 'documents') continue;
    const g = groupOf(pathOf(e.source)), t = groupOf(pathOf(e.target));
    const key = `${g}->${t}`;
    documentsTargets[key] = (documentsTargets[key] || 0) + 1;
  }

  // ---------- file stats ----------
  const filesPerGroup = {};
  for (const [g, ids] of Object.entries(directoryGroups)) filesPerGroup[g] = ids.length;
  const filesPerWeek = {};
  for (const [g, ids] of Object.entries(weekGroups)) filesPerWeek[g] = ids.length;
  const nodeTypeCounts = {};
  for (const [t, ids] of Object.entries(nodeTypeGroups)) nodeTypeCounts[t] = ids.length;

  const result = {
    scriptCompleted: true,
    commonPrefix: prefixStr || '(none)',
    directoryGroups,
    weekGroups,
    nodeTypeGroups,
    crossCategoryEdges,
    interGroupImports,
    interWeekImports,
    intraGroupDensity,
    intraWeekDensity,
    patternMatches,
    filePatterns,
    weekRoles,
    deploymentTopology,
    dataPipeline,
    docCoverage,
    dependencyDirection,
    similarToEdgeCount: similarEdges.length,
    similarToByWeekPair: similarByWeek,
    documentsTargets,
    fileStats: {
      totalFileNodes: fileNodes.length,
      filesPerGroup,
      filesPerWeek,
      nodeTypeCounts,
    },
    fileFanIn: fanIn,
    fileFanOut: fanOut,
  };

  fs.writeFileSync(outPath, JSON.stringify(result, null, 1));
  console.log(`OK: analyzed ${fileNodes.length} file nodes, ${importEdges.length} import edges, ${allEdges.length} all edges -> ${outPath}`);
}

try {
  main();
  process.exit(0);
} catch (err) {
  console.error('FATAL:', err && err.stack || err);
  process.exit(1);
}

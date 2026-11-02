# Qt / OCCT 原生窗口尺寸同步

记录于 2026-10-29，模板真实视窗验收期间。

现象：Qt 停靠布局扩大中央窗口后，OCCT 仍只在原先 320×240 区域绘制，其余部分黑屏。仅调用 `V3d_View::MustBeResized()` 不足以刷新 `Xw_Window` 的缓存尺寸。

处理：在 resize 以及 paint 阶段调用 `view->Window()->DoResize()`，再调用 `MustBeResized()`。paint 阶段的重复同步覆盖 Qt 事件先于原生窗口更新的时序。

回归：真实 X11/GLX 环境中，检查 OCCT 导出图像尺寸等于 QWidget 尺寸乘设备像素比例，并在 1×/2×下验证鼠标选面和拓扑树同步。截图必须人工查看，避免以“有非背景像素”误判为完整视窗正常。

环境：沙箱中不能连接 Xvfb socket 时，使用获准的执行环境运行真实视窗测试。Qt offscreen 测试仍用于参数和适配逻辑，不代替真实渲染。

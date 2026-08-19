# SoftRenderer

一个使用 C++17 编写的 CPU 软件渲染器，用于学习并实现实时渲染管线中的核心算法。项目以 OBJ/TGA 资源为输入，输出 TGA 图像；不依赖图形 API。

## 已实现功能

- OBJ 多模型加载，以及漫反射、法线和高光 TGA 贴图
- 齐次裁剪空间的六平面三角形裁剪与重三角化
- 透视正确插值、边函数光栅化、像素中心采样、Top-Left 填充规则和 Z-buffer
- Repeat/Clamp 寻址与双线性纹理过滤
- 切线空间法线贴图与 Blinn-Phong 光照
- 方向光两阶段阴影：根据场景 AABB 自适应拟合正交阴影视锥
- PCF 软阴影，以及基于 texel 尺寸的常量 bias、斜率 bias 和法线偏移
- 包含独立的渲染器单元测试

## 构建

环境要求：Visual Studio 2022、MSVC v143 和 Windows SDK。

在 Visual Studio 中打开 SoftRenderer.sln，选择 Release | x64 并生成解决方案；也可以在开发者命令行中执行：

    msbuild SoftRenderer.sln /m /p:Configuration=Release /p:Platform=x64

生成的渲染器位于 bin/Release_x64/SoftRenderer/SoftRenderer.exe。

## 运行

从仓库根目录运行。下面的例子同时加载角色和地面，并将结果写入 results/diablo：

    .\bin\Release_x64\SoftRenderer\SoftRenderer.exe ^
      --size 1280x720 ^
      --shadow-size 2048x2048 ^
      --output results\diablo ^
      obj\diablo3_pose\diablo3_pose.obj ^
      obj\floor.obj

PowerShell 中请将续行符 ^ 改为反引号。

程序会自动创建输出目录，并写出：

- framebuffer.tga：最终颜色结果
- shadowmap.tga：方向光深度图，可用于检查阴影覆盖范围
- zbuffer.tga：相机深度图，可用于检查遮挡关系

完整选项可通过以下命令查看：

    .\bin\Release_x64\SoftRenderer\SoftRenderer.exe --help

其中最常用的调参项为：

| 选项 | 作用 |
| --- | --- |
| --size <w>x<h> | 最终图像分辨率 |
| --shadow-size <w>x<h> | 阴影贴图分辨率 |
| --pcf-radius <0-8> | PCF 半径；0 表示关闭过滤 |
| --bias-constant <texels> | 常量深度 bias |
| --bias-slope <texels> | 与入射角相关的斜率 bias |
| --bias-max-slope <value> | 斜率项上限 |
| --normal-offset <texels> | 阴影查询点沿几何法线的偏移 |
| --output <directory> | 结果输出目录 |

默认 bias 参数针对本仓库示例场景设置。遇到阴影痤疮（acne）时，应优先提高阴影分辨率或小幅调整 --bias-constant；不要单独将固定 bias 调得过大，否则会产生悬浮阴影（peter-panning）。

## 测试

测试覆盖裁剪、Top-Left 规则、纹理采样、阴影可见性和 bias 尺度等基础行为。生成解决方案后执行：

    .\bin\Release_x64\SoftRendererTests\SoftRendererTests.exe

预期输出：

    All renderer tests passed.




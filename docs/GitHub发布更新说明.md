# GitHub 发布更新说明

本文档约定 CineVault Windows 安装包的公开发布步骤，供自动更新功能读取 GitHub `latest release` 使用。

## 发布前提

- 仓库：`luojiang419/dit-tools`
- 自动更新只读取公开 GitHub Release
- 安装包资产名称必须严格符合：
  - `CineVault-Setup-vX.Y.Z.exe`
- 应用启动时会请求：
  - `https://api.github.com/repos/luojiang419/dit-tools/releases/latest`

## 标准发布步骤

1. 生成真实工作流安装包

```powershell
powershell -ExecutionPolicy Bypass -File tool/build_windows.ps1 -RealWorkflow
```

2. 记录输出版本

```text
output/vX.Y.Z/CineVault-Setup-vX.Y.Z.exe
```

3. 推送主分支代码

```powershell
git push origin main
```

4. 创建同版本 tag

```powershell
git tag vX.Y.Z
git push origin vX.Y.Z
```

5. 创建公开 Release 并上传安装包

```powershell
gh release create vX.Y.Z "output/vX.Y.Z/CineVault-Setup-vX.Y.Z.exe" `
  --repo luojiang419/dit-tools `
  --title "CineVault vX.Y.Z" `
  --notes "Windows 安装包发布，供应用内自动更新使用。"
```

## 发布后检查

- GitHub Release 页面可见最新版本
- 资产名称与版本号完全一致
- 旧版本启动后可检测到更高版本并开始后台下载
- 下载完成后出现统一确认框：
  - `立即更新`
  - `下次启动更新`

## 说明

- 当前更新闭环只覆盖 Windows 安装器更新
- 本次未引入 GitHub Actions、差分更新、私有仓库 Token 或签名校验体系
- 如果最新 Release 不存在、缺少匹配资产或网络失败，应用只提示中文状态，不中断启动


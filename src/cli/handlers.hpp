#pragma once

namespace noldoj::cli {

struct Runtime;
struct Args;

// 认证 / 本地配置
int cmdAuthStatus(Runtime&, const Args&);
int cmdSetToken(Runtime&, const Args&);
int cmdSudoLogin(Runtime&, const Args&);

// 提交 / 运行
int cmdSubmit(Runtime&, const Args&);
int cmdIdeRun(Runtime&, const Args&);
int cmdWatchSubmission(Runtime&, const Args&);

// 上传
int cmdImportProblem(Runtime&, const Args&);
int cmdUploadTestdata(Runtime&, const Args&);
int cmdUploadTestdataZip(Runtime&, const Args&);
int cmdUploadFile(Runtime&, const Args&);

// 站点 / 配置
int cmdSiteList(Runtime&, const Args&);
int cmdSiteAdd(Runtime&, const Args&);
int cmdSiteRemove(Runtime&, const Args&);
int cmdSiteUse(Runtime&, const Args&);
int cmdSiteShow(Runtime&, const Args&);
int cmdConfigShow(Runtime&, const Args&);
int cmdConfigPath(Runtime&, const Args&);

}  // namespace noldoj::cli

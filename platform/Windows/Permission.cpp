#include <windows.h>
#include <shobjidl.h>

namespace ceh::platform {

void setupWindowsApplication() {
#ifdef _WIN32
    // 设置 Windows AppUserModelID 保证任务栏图标独立并且正确渲染
    ::SetCurrentProcessExplicitAppUserModelID(L"CEH.Windows_Uia.Ai.V1");
#endif
}

} // namespace ceh::platform

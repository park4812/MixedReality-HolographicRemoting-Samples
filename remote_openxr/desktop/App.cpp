//*********************************************************
//    Copyright (c) Microsoft. All rights reserved.
//
//    Apache 2.0 License
//
//    You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
//    implied. See the License for the specific language governing
//    permissions and limitations under the License.
//
//*********************************************************

#include "pch.h"
#include "OpenXrProgram.h"

#include <SampleShared/CommandLineUtility.h>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
constexpr const char* ProgramName = "SampleRemoteOpenXr_win32";
#else
constexpr const char* ProgramName = "SampleRemoteOpenXr_uwp";
#endif

int __stdcall WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    try {
        sample::AppOptions options;
        sample::ParseCommandLine(options);
        options.listen = true;
        options.noUserWait = true;
        options.allowCertificateNameMismatch = true;
        options.allowUnverifiedCertificateChain = true;
        //options.isStandalone = true;
        //options.secureConnection = true;

        auto graphics = sample::CreateCubeGraphics();
        auto program = sample::CreateOpenXrProgram(ProgramName, std::move(graphics), options);
        program->Run();
    } catch (const std::exception& ex) {
        DEBUG_PRINT("Unhandled Exception: %s\n", ex.what());
        return 1;
    } catch (...) {
        DEBUG_PRINT("Unhandled Exception\n");
        return 1;
    }
    return 0;
}

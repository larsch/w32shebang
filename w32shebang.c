#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>

#define TCHARSIZE sizeof(TCHAR)
#define PATHSEP _T(';')

static LPCTSTR BuiltinAssoc = _T(".sh\0sh\0bash\0\0" \
   ".rb\0ruby\0\0" \
   ".rbw\0rubyw\0\0" \
   ".pl\0perl\0\0" \
   ".py\0python\0python3\0python2\0\0\0");

/* Allocate a buffer for a string of 'n' characters */
static LPTSTR AllocString(size_t n)
{
   return (LPTSTR)LocalAlloc(LMEM_FIXED, TCHARSIZE * n);
}

/* Return a pointer to the next string after the next \0-char */
static LPCTSTR NextString(LPCTSTR str)
{
   while (*str)
      str++;
   str++;
   return str;
}

/* Formats a message (allocates buffer) */
static LPTSTR SPrintF(LPTSTR pMessage, ...)
{
   LPTSTR pBuffer = NULL;
   va_list args = NULL;
   va_start(args, pMessage);
   FormatMessage(FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ALLOCATE_BUFFER,
      pMessage, 0, 0, (LPTSTR)&pBuffer, 0, &args);
   va_end(args);
   return pBuffer;
}

/* Formats a message using va_list (allocates buffer) */
static LPTSTR SPrintFV(LPCTSTR message, va_list args)
{
   LPTSTR pBuffer = NULL;
   FormatMessage(FORMAT_MESSAGE_FROM_STRING |
      FORMAT_MESSAGE_ALLOCATE_BUFFER,
      message,
      0,
      0,
      (LPTSTR)&pBuffer,
      0,
      &args);
   return pBuffer;
}

/* Prints a message to STDOUT */
static void Print(LPCTSTR pMessage)
{
   DWORD written;
   WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), pMessage, lstrlen(pMessage), &written, NULL);
}

/* Prints a formatted message to STDOUT, using va_list */
static void PrintFV(LPCTSTR message, va_list args)
{
   LPTSTR formattedMessage = SPrintFV(message, args);
   Print(formattedMessage);
   LocalFree(formattedMessage);
}

/* Prints a formatted message to STDOUT */
static void PrintF(LPCTSTR message, ...)
{
   va_list args = NULL;
   va_start(args, message);
   PrintFV(message, args);
   va_end(args);
}

/* Gets the error message for a specified error code (allocates buffer) */
static LPTSTR GetErrorText(DWORD dwError)
{
   LPTSTR pBuffer = NULL;
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwError, LANG_NEUTRAL, (LPTSTR)&pBuffer, 0, NULL);
   return pBuffer;
}

/* Console control event handler */
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
   switch (dwCtrlType) {
   case CTRL_C_EVENT:
   case CTRL_BREAK_EVENT:
      return TRUE;
   case CTRL_CLOSE_EVENT:
   case CTRL_LOGOFF_EVENT:
   case CTRL_SHUTDOWN_EVENT:
   default:
      return FALSE;
   }
}

/* Prints error and error message (from GetLastError) and exits */
static void AbortLastError(LPCTSTR message)
{
   LPTSTR errorText = GetErrorText(GetLastError());
   PrintF(_T("%1: %2"), message, errorText);
   LocalFree(errorText);
   ExitProcess(1);
}

/* Prints error and exits */
static void Abort(LPCTSTR message)
{
   PrintF(_T("%1\n"), message);
   ExitProcess(1);
}

/* Generate a \0-separated list of element in the PATHEXT environment variable */
static LPCTSTR GeneratePathExt()
{
   static TCHAR pathext[MAX_PATH];
   DWORD Length = GetEnvironmentVariable(_T("PATHEXT"), pathext, MAX_PATH);
   if (Length > MAX_PATH)
      Abort(_T("PATHEXT too long"));
   LPTSTR p = pathext;
   while (*p) {
      if (*p == PATHSEP)
         *p = 0;
      p++;
   }
   p[1] = 0;
   return pathext;
}

/* Get the PATHEXT environment variable as \0-separated list */
static LPCTSTR GetPathExt()
{
   static LPCTSTR pathExt = NULL;
   if (pathExt == NULL)
      pathExt = GeneratePathExt();
   return pathExt;
}

/* Find the program associated with a specific extension */
BOOL FindAssoc(LPTSTR Extension, LPTSTR _Out_ Program)
{
   DWORD outLen = MAX_PATH;
   HRESULT hr = AssocQueryString(ASSOCF_INIT_IGNOREUNKNOWN, ASSOCKEY_APP, Extension, NULL, Program, &outLen);
   return (hr == S_OK) && PathFileExists(Program);
}

/* Find a file in PATH trying each possible extension in PATHEXT */
static BOOL SearchPathWithPathExt(LPCTSTR Program, LPTSTR _Out_ ProgramPath)
{
   LPCTSTR pathext = GetPathExt();
   if (!pathext)
      return FALSE;
   while (*pathext) {
      TCHAR candidate[MAX_PATH];
      lstrcpy(candidate, Program);
      PathAddExtension(candidate, pathext);
      DWORD Result = SearchPath(NULL, candidate, NULL, MAX_PATH, ProgramPath, NULL);
      if (Result != 0)
         return TRUE;
      pathext = NextString(pathext);
   }
   return FALSE;
}

/* Find a file in PATH using PATHEXT if necessary */
static BOOL SearchPathAllowPathExt(LPCTSTR Program, LPTSTR _Out_ ProgramPath)
{
   DWORD Result = SearchPath(NULL, Program, NULL, MAX_PATH, ProgramPath, NULL);
   if (Result == 0)
      return SearchPathWithPathExt(Program, ProgramPath);
   else
      return TRUE;
}

static void GetCommand(LPCTSTR CommandLine, LPTSTR _Out_ Command) 
{ 
   LPTSTR Arguments = PathGetArgs(CommandLine);
   ptrdiff_t CommandLength = Arguments - CommandLine;
   while (CommandLength > 0 && CommandLine[CommandLength - 1] == _T(' '))
      CommandLength--;
   lstrcpyn(Command, CommandLine, (int)(CommandLength + 1));
}

static LPTSTR lstrchr(LPTSTR String, TCHAR Char) {
   while (*String) {
      if (*String == Char)
         return String;
      ++String;
   }
   return NULL;
}

static BOOL QuerySheBang(LPCTSTR Path, LPTSTR _Out_ Program)
{
   HANDLE hFile = CreateFile(Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
   if (hFile == INVALID_HANDLE_VALUE)
      return FALSE;

   char SheBangLineChars[MAX_PATH];
   DWORD BytesRead;
   if (!ReadFile(hFile, SheBangLineChars, sizeof(SheBangLineChars)-1, &BytesRead, NULL))
      AbortLastError(_T("ReadFile"));
   CloseHandle(hFile);

   TCHAR SheBangLine[MAX_PATH];
   int Length = MultiByteToWideChar(CP_UTF8, 0, SheBangLineChars, BytesRead, SheBangLine, MAX_PATH-1);
   if (Length == 0)
      AbortLastError(_T("MultiByteToWideChar"));
   SheBangLine[Length] = _T('\0');

   if (SheBangLine[0] == _T('#') && SheBangLine[1] == _T('!')) {
      LPTSTR NewLine = lstrchr(SheBangLine, _T('\n'));
      if (NewLine) {
         if (NewLine > SheBangLine && NewLine[-1] == _T('\r'))
            NewLine--;
         *NewLine = _T('\0');
      }
      LPTSTR CommandLine = SheBangLine + 2;
      LPTSTR Arguments = PathGetArgs(CommandLine);
      TCHAR Command[MAX_PATH];
      GetCommand(CommandLine, Command);
      if (PathFileExists(Command)) {
         lstrcpy(Program, Command); // raw from shebang if it exists
         return TRUE;
      } else {
         LPTSTR FileName = PathFindFileName(Command);
         if (SearchPathAllowPathExt(FileName, Program)) {
            return TRUE; // file name (without path) found using PATH/PATHEXT
         } else if (lstrcmp(FileName, _T("env"))) {
            CommandLine = Arguments;
            Arguments = PathGetArgs(CommandLine);
            GetCommand(CommandLine, Command);
            if (SearchPathAllowPathExt(Command, Program)) {
               return TRUE; // command after 'env' found using PATH/PATHEXT
            }
         }
      }
   }

   return FALSE;
}

static BOOL FindScriptBySheBang(LPTSTR _Out_ Path, LPTSTR _Out_ Program) {
   DWORD Result = GetModuleFileName(NULL, Path, MAX_PATH);
   if (GetLastError() == ERROR_INSUFFICIENT_BUFFER || Result == MAX_PATH)
      Abort(_T("Insufficient buffer"));
   PathRemoveExtension(Path);
   if (PathFileExists(Path)) {
      if (QuerySheBang(Path, Program)) {
         return TRUE;
      }
   } else {
      Result = GetModuleFileName(NULL, Path, MAX_PATH);
      if (GetLastError() == ERROR_INSUFFICIENT_BUFFER || Result == MAX_PATH)
         Abort(_T("Insufficient buffer"));
      LPCTSTR pathExt = GetPathExt();
      while (*pathExt) {
         PathRenameExtension(Path, pathExt);
         if (PathFileExists(Path)) {
            if (QuerySheBang(Path, Program)) {
               return TRUE;
            }
         }
         pathExt = NextString(pathExt);
      }
   }
   return FALSE;
}

/* Find script and program using the PATHEXT environment variable */
static BOOL FindScriptByPathExt(LPTSTR _Out_ Path, LPTSTR Program)
{
   DWORD Result = GetModuleFileName(NULL, Path, MAX_PATH);
   if (GetLastError() == ERROR_INSUFFICIENT_BUFFER || Result == MAX_PATH)
      Abort(_T("Insufficient buffer"));
   LPCTSTR pathExt = GetPathExt();
   while (*pathExt) {
      PathRenameExtension(Path, pathExt);
      if (PathFileExists(Path)) {
         LPTSTR Extension = PathFindExtension(Path);
         if (Extension != NULL && *Extension) {
            if (FindAssoc(Extension, Program)) {
               return TRUE;
            }
         }
      }
      pathExt = NextString(pathExt);
   }
   return FALSE;
}

/* Find script and program using built-in defaults (e.g. shell, perl, python and ruby) */
static BOOL FindScriptByBuiltin(LPTSTR _Out_ ScriptPath, LPTSTR _Out_ ProgramPath)
{
   DWORD Result = GetModuleFileName(NULL, ScriptPath, MAX_PATH);
   if (GetLastError() == ERROR_INSUFFICIENT_BUFFER || Result == MAX_PATH)
      Abort(_T("Insufficient buffer"));
   LPCTSTR ptr = BuiltinAssoc;
   while (*ptr) {
      LPCTSTR ext = ptr;
      PathRenameExtension(ScriptPath, ext);
      BOOL exists = PathFileExists(ScriptPath);
      LPCTSTR Program = NextString(ptr);
      while (*Program) {
         if (exists && SearchPathAllowPathExt(Program, ProgramPath))
            return TRUE;
         Program = NextString(Program);
      }
      ptr = Program + 1;
   }
   return FALSE;
}

/* Execute script with found program, passing on arguments */
static void ExecWith(LPCTSTR ChildProgram, LPCTSTR ScriptPath)
{
   LPCTSTR SelfCommandLine = GetCommandLine();
   LPCTSTR Arguments = PathGetArgs(SelfCommandLine);
   LPTSTR ChildCommandLine = AllocString(lstrlen(ChildProgram) + 3 + lstrlen(ScriptPath) + 3 + lstrlen(Arguments) + 1);
   
   lstrcpy(ChildCommandLine, ChildProgram);
   PathQuoteSpaces(ChildCommandLine);
   lstrcat(ChildCommandLine, _T(" "));
   LPTSTR ScriptArg = ChildCommandLine + lstrlen(ChildCommandLine);
   lstrcpy(ScriptArg, ScriptPath);
   PathQuoteSpaces(ScriptArg);
   lstrcat(ChildCommandLine, _T(" "));
   lstrcat(ChildCommandLine, Arguments);

   STARTUPINFO StartupInfo;
   PROCESS_INFORMATION ProcessInformation;
   SecureZeroMemory(&StartupInfo, sizeof(StartupInfo));
   StartupInfo.cb = sizeof(StartupInfo);

   if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE))
      AbortLastError(_T("SetConsoleCtrlHandler"));
   if (!CreateProcess(ChildProgram, ChildCommandLine, NULL, NULL, TRUE, 0, NULL, NULL, &StartupInfo, &ProcessInformation))
      AbortLastError(_T("Failed to start process"));

   WaitForSingleObject(ProcessInformation.hProcess, INFINITE);
   if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE))
      AbortLastError(_T("SetConsoleCtrlHandler"));

   DWORD dwExitCode;
   if (!GetExitCodeProcess(ProcessInformation.hProcess, &dwExitCode))
      AbortLastError(_T("GetExitCodeProcess"));
   ExitProcess(dwExitCode);
}

/* Main entry point */
void Main()
{
   TCHAR ScriptPath[MAX_PATH];
   TCHAR Program[MAX_PATH];
   if (FindScriptBySheBang(ScriptPath, Program)) {
      ExecWith(Program, ScriptPath);
   } if (FindScriptByPathExt(ScriptPath, Program)) {
      ExecWith(Program, ScriptPath);
   } else {
      if (FindScriptByBuiltin(ScriptPath, Program)) {
         ExecWith(Program, ScriptPath);
      } else {
         Abort(_T("Command not found."));
      }
   }
}

$musicPath = 'D:\Codex\libgme_inno\test.nsf'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class GmeInnoStatic
{
    [DllImport(@"D:\Codex\libgme_inno\build-win32-static\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    public static extern int GMEInnoOpenFileTrackW(string path, int sampleRate, int trackIndex);

    [DllImport(@"D:\Codex\libgme_inno\build-win32-static\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int GMEInnoGetTrackCount();

    [DllImport(@"D:\Codex\libgme_inno\build-win32-static\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int GMEInnoPlay();

    [DllImport(@"D:\Codex\libgme_inno\build-win32-static\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern void GMEInnoStop();

    [DllImport(@"D:\Codex\libgme_inno\build-win32-static\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern void GMEInnoClose();
}
'@

$open = [GmeInnoStatic]::GMEInnoOpenFileTrackW($musicPath, 44100, 0)
if ($open -eq 0) {
    throw 'OPEN_FAILED'
}

$tracks = [GmeInnoStatic]::GMEInnoGetTrackCount()
$play = [GmeInnoStatic]::GMEInnoPlay()
Start-Sleep -Milliseconds 300
[GmeInnoStatic]::GMEInnoStop()
[GmeInnoStatic]::GMEInnoClose()

Write-Output ('STATIC_OPEN_OK tracks=' + $tracks + ' play=' + $play)

$dllPath = 'D:\Codex\libgme_inno\build-win32\inno\Release\gme_inno.dll'
$musicPath = 'D:\Codex\libgme_inno\test.nsf'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class GmeInno
{
    [DllImport(@"D:\Codex\libgme_inno\build-win32\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    public static extern int GMEInnoOpenFileW(string path, int sampleRate);

    [DllImport(@"D:\Codex\libgme_inno\build-win32\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    public static extern int GMEInnoOpenFileTrackW(string path, int sampleRate, int trackIndex);

    [DllImport(@"D:\Codex\libgme_inno\build-win32\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int GMEInnoStartTrack(int trackIndex);

    [DllImport(@"D:\Codex\libgme_inno\build-win32\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int GMEInnoPlay();

    [DllImport(@"D:\Codex\libgme_inno\build-win32\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern void GMEInnoStop();

    [DllImport(@"D:\Codex\libgme_inno\build-win32\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern void GMEInnoClose();

    [DllImport(@"D:\Codex\libgme_inno\build-win32\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int GMEInnoGetTrackCount();

    [DllImport(@"D:\Codex\libgme_inno\build-win32\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int GMEInnoGetLastErrorLength();

    [DllImport(@"D:\Codex\libgme_inno\build-win32\inno\Release\gme_inno.dll", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    public static extern int GMEInnoGetLastErrorW(StringBuilder buffer, int capacity);
}
'@

$open = [GmeInno]::GMEInnoOpenFileTrackW($musicPath, 44100, 0)
if ($open -eq 0) {
    $capacity = [Math]::Max([GmeInno]::GMEInnoGetLastErrorLength(), 256)
    $buffer = New-Object System.Text.StringBuilder $capacity
    [void][GmeInno]::GMEInnoGetLastErrorW($buffer, $buffer.Capacity)
    throw ('OPEN_FAILED: ' + $buffer.ToString())
}

$tracks = [GmeInno]::GMEInnoGetTrackCount()
$start = [GmeInno]::GMEInnoStartTrack(0)
$play = [GmeInno]::GMEInnoPlay()
Start-Sleep -Milliseconds 400
[GmeInno]::GMEInnoStop()
[GmeInno]::GMEInnoClose()

Write-Output ('OPEN_OK tracks=' + $tracks + ' start=' + $start + ' play=' + $play)

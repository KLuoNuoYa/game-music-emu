[Setup]
AppName=libgme Inno Demo
AppVersion=1.0
DefaultDirName={autopf}\libgme-inno-demo
DefaultGroupName=libgme-inno-demo
Uninstallable=no
Compression=lzma
SolidCompression=yes

[Files]
Source: "gme_inno.dll"; DestDir: "{tmp}"; Flags: dontcopy
Source: "test.nsf"; DestDir: "{tmp}"; Flags: dontcopy

[Code]
function GMEInnoOpenFileW(Path: string; SampleRate: Integer): Integer;
  external 'GMEInnoOpenFileW@files:gme_inno.dll stdcall delayload';
function GMEInnoOpenFileTrackW(Path: string; SampleRate: Integer; TrackIndex: Integer): Integer;
  external 'GMEInnoOpenFileTrackW@files:gme_inno.dll stdcall delayload';
function GMEInnoStartTrack(TrackIndex: Integer): Integer;
  external 'GMEInnoStartTrack@files:gme_inno.dll stdcall delayload';
function GMEInnoPlay: Integer;
  external 'GMEInnoPlay@files:gme_inno.dll stdcall delayload';
procedure GMEInnoPause;
  external 'GMEInnoPause@files:gme_inno.dll stdcall delayload';
procedure GMEInnoStop;
  external 'GMEInnoStop@files:gme_inno.dll stdcall delayload';
procedure GMEInnoClose;
  external 'GMEInnoClose@files:gme_inno.dll stdcall delayload';
procedure GMEInnoSetLoop(Enabled: Integer);
  external 'GMEInnoSetLoop@files:gme_inno.dll stdcall delayload';
procedure GMEInnoSetVolume(VolumePercent: Integer);
  external 'GMEInnoSetVolume@files:gme_inno.dll stdcall delayload';
function GMEInnoGetLastErrorW(var Buffer: string; Capacity: Integer): Integer;
  external 'GMEInnoGetLastErrorW@files:gme_inno.dll stdcall delayload';
function GMEInnoGetLastErrorLength: Integer;
  external 'GMEInnoGetLastErrorLength@files:gme_inno.dll stdcall delayload';

function ReadGmeError: string;
var
  Buffer: string;
begin
  SetLength(Buffer, GMEInnoGetLastErrorLength);
  GMEInnoGetLastErrorW(Buffer, Length(Buffer));
  while (Length(Buffer) > 0) and (Buffer[Length(Buffer)] = #0) do
    SetLength(Buffer, Length(Buffer) - 1);
  Result := Buffer;
end;

procedure InitializeWizard;
var
  MusicPath: string;
begin
  ExtractTemporaryFile('test.nsf');

  MusicPath := ExpandConstant('{tmp}\test.nsf');
  if GMEInnoOpenFileTrackW(MusicPath, 44100, 0) = 0 then
    MsgBox('Open failed: ' + ReadGmeError, mbError, MB_OK)
  else begin
    GMEInnoSetLoop(1);
    GMEInnoSetVolume(70);
    GMEInnoStartTrack(0);
    GMEInnoPlay;
  end;
end;

procedure DeinitializeSetup;
begin
  GMEInnoStop;
  GMEInnoClose;
end;

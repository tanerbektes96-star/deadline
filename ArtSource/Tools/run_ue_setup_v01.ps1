# Rebuilds the V01 / shared NPC animation setup from scratch.
# Clears the generated folders on disk (to the Recycle Bin) while the editor is closed,
# then runs ue_setup_v01.py. Close Unreal Editor first.
$ErrorActionPreference = "Stop"
$Project = Resolve-Path "$PSScriptRoot\..\.."
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    throw "Unreal Editor is running. Close it first."
}

Add-Type -AssemblyName Microsoft.VisualBasic
foreach ($rel in "Content\Deadline\Characters\Shared", "Content\Deadline\Characters\NPC\V01",
                 "Content\Deadline\Maps\Test") {
    $dir = Join-Path $Project $rel
    if (Test-Path $dir) {
        [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteDirectory($dir, 'OnlyErrorDialogs', 'SendToRecycleBin')
        Write-Host "recycled $rel"
    }
}

& $Editor "$Project\DEADLINE_.uproject" "-ExecutePythonScript=$PSScriptRoot\ue_setup_v01.py" `
    -unattended -nosplash -nop4 -RenderOffscreen -stdout -FullStdOutLogOutput |
    Select-String -Pattern "V01SETUP|LogPython: Error" |
    ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogPython: Warning: ', '' }

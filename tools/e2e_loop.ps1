# Runs the end-to-end test N times and reports any run that fails or exits
# abnormally (used to hunt intermittent problems). Usage:
#   powershell -ExecutionPolicy Bypass -File tools/e2e_loop.ps1 -Runs 15
param([int]$Runs = 10)
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "build\e2e_section12.exe"
$server = Join-Path $root "build\cafeteria_server.exe"
$bad = 0
for ($i = 1; $i -le $Runs; $i++) {
    $log = Join-Path $root "build\e2e_loop_$i.log"
    $p = Start-Process -FilePath $exe -ArgumentList "`"$server`"", "`"$root`"" -NoNewWindow -PassThru `
        -RedirectStandardOutput $log -RedirectStandardError "$log.err"
    $null = $p.Handle  # cache the handle so ExitCode is available after exit
    if (-not $p.WaitForExit(90000)) { $p.Kill(); $code = "TIMEOUT" } else { $code = $p.ExitCode }
    $fails = (Select-String -Path $log -Pattern "^FAIL" | Measure-Object).Count
    if ($code -ne 0 -or $fails -ne 0) {
        $bad++
        $tail = (Get-Content $log -Tail 2) -join " | "
        Write-Output ("run {0}: exit={1} (0x{2:X}) fails={3} tail: {4}" -f $i, $code, [int64]$code, $fails, $tail)
        Select-String -Path $log -Pattern "^FAIL" | Select-Object -First 3 | ForEach-Object { "   " + $_.Line.Substring(0, [Math]::Min(300, $_.Line.Length)) }
    } else {
        Remove-Item $log, "$log.err" -ErrorAction SilentlyContinue
    }
    Get-Process cafeteria_server -ErrorAction SilentlyContinue | Stop-Process -Force
}
Write-Output "bad runs: $bad/$Runs"

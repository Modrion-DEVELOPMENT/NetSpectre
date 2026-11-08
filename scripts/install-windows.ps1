# SPDX-License-Identifier: AGPL-3.0-only
# Requires Windows PowerShell 5.1 (included with Windows 10/11).
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectDir = Split-Path $PSScriptRoot -Parent
$installDir = Join-Path $env:LOCALAPPDATA 'Modrion NetSpectre'
$binDir = Join-Path $installDir 'bin'
$appDir = Join-Path $installDir 'app'
function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed (exit $LASTEXITCODE)." }
}
function Refresh-Path {
    $env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')
}
function Find-WinGet {
    $command = Get-Command winget.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $package = Get-AppxPackage -Name Microsoft.DesktopAppInstaller | Select-Object -First 1
    if ($package) {
        $candidate = Join-Path $package.InstallLocation 'winget.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}
function Get-CompilerInstallation {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -eq 0 -and $found) { return ($found | Select-Object -First 1) }
    }
    return $null
}
try {
    if ([Environment]::OSVersion.Version.Build -lt 19041) { throw 'Windows 10 version 2004 or newer / Windows 11 is required.' }
    Write-Host 'Modrion NetSpectre installer'
    Write-Host 'Downloads missing build tools, builds NetSpectre, and adds netspro to your user PATH.'
    Write-Host 'Internet access and several GB of free space may be needed. Approve Windows installer prompts.'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $winget = Find-WinGet
    if (-not $winget) {
        Write-Host 'Installing WinGet using the official Microsoft.WinGet.Client module...'
        Install-PackageProvider -Name NuGet -MinimumVersion 2.8.5.201 -Scope CurrentUser -Force | Out-Null
        Install-Module Microsoft.WinGet.Client -Repository PSGallery -Scope CurrentUser -Force -AllowClobber
        Import-Module Microsoft.WinGet.Client
        Repair-WinGetPackageManager -Force -Latest
        Refresh-Path
        $winget = Find-WinGet
        if (-not $winget) { throw 'WinGet installation did not finish. Restart Windows and rerun install.bat.' }
    }
    $cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if (-not $cmake) {
        Invoke-Checked $winget @('install','--exact','--id','Kitware.CMake','--source','winget','--accept-source-agreements','--accept-package-agreements')
        Refresh-Path
        $cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
    }
    $cmakePath = if ($cmake) { $cmake.Source } else { Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe' }
    if (-not (Test-Path -LiteralPath $cmakePath)) { throw 'CMake is unavailable. Restart Windows and rerun install.bat.' }
    $compiler = Get-CompilerInstallation
    if (-not $compiler) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        $existing = $null
        if (Test-Path -LiteralPath $vswhere) {
            $existing = & $vswhere -latest -products '*' -version '[17.0,18.0)' -property installationPath
        }
        if ($existing) {
            $setup = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\setup.exe'
            $modify = Start-Process -FilePath $setup -Verb RunAs -Wait -PassThru -ArgumentList @(
                'modify','--installPath',('"' + ($existing | Select-Object -First 1) + '"'),
                '--passive','--norestart','--add','Microsoft.VisualStudio.Workload.VCTools','--includeRecommended')
            if ($modify.ExitCode -eq 3010) { throw 'C++ tools need a Windows restart. Restart and rerun install.bat.' }
            if ($modify.ExitCode -ne 0) { throw "C++ workload installation failed (exit $($modify.ExitCode))." }
        } else {
        Invoke-Checked $winget @('install','--exact','--id','Microsoft.VisualStudio.2022.BuildTools','--source','winget',
            '--accept-source-agreements','--accept-package-agreements','--override',
            '--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended')
        }
        $compiler = Get-CompilerInstallation
        if (-not $compiler) { throw 'C++ Build Tools are not ready. Restart Windows and rerun install.bat.' }
    }
    $buildDir = Join-Path $projectDir 'build\install-windows'
    $outputDir = Join-Path $buildDir 'bin'
    Invoke-Checked $cmakePath @('-S',$projectDir,'-B',$buildDir,'-G','Visual Studio 17 2022','-A','x64',
        "-DCMAKE_GENERATOR_INSTANCE=$compiler","-DNETSPRO_OUTPUT_DIRECTORY=$outputDir")
    Invoke-Checked $cmakePath @('--build',$buildDir,'--config','Release','--parallel','2')
    New-Item -ItemType Directory -Force -Path $appDir,$binDir,(Join-Path $appDir 'catalog'),(Join-Path $appDir 'themes') | Out-Null
    Copy-Item -LiteralPath (Join-Path $outputDir 'Netspro.exe') -Destination (Join-Path $appDir 'Netspro.exe') -Force
    foreach ($theme in Get-ChildItem -LiteralPath (Join-Path $projectDir 'themes') -Filter '*.json') {
        $target = Join-Path (Join-Path $appDir 'themes') $theme.Name
        if (-not (Test-Path -LiteralPath $target)) { Copy-Item -LiteralPath $theme.FullName -Destination $target }
    }
    foreach ($file in @('third_party.json','homebrew.json')) {
        Copy-Item -LiteralPath (Join-Path (Join-Path $projectDir 'catalog') $file) -Destination (Join-Path $appDir 'catalog') -Force
    }
    foreach ($file in @('LICENSE','ADDITIONAL_TERMS.md')) { Copy-Item -LiteralPath (Join-Path $projectDir $file) -Destination $appDir -Force }
    @'
@echo off
setlocal
pushd "%~dp0..\app" || exit /b 1
"%~dp0..\app\Netspro.exe" %*
set "NETSPRO_EXIT=%ERRORLEVEL%"
popd
exit /b %NETSPRO_EXIT%
'@ | Set-Content -LiteralPath (Join-Path $binDir 'netspro.cmd') -Encoding ASCII
    $userPath = [Environment]::GetEnvironmentVariable('Path','User')
    $parts = @($userPath -split ';' | Where-Object { $_ })
    if ($parts -notcontains $binDir) {
        [Environment]::SetEnvironmentVariable('Path',(($parts + $binDir) -join ';'),'User')
    }
    Write-Host "Installed successfully in $appDir"
    Write-Host 'Close and reopen Command Prompt / Windows Terminal and type netspro (or Netspro).'
    Write-Host 'If PATH has not refreshed, sign out and back in. Your existing configuration and themes were preserved.'
    exit 0
} catch {
    Write-Host "Installation failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

param(
	[ValidateSet("Debug", "Release")]
	[string]$Configuration = "Debug",

	[ValidateSet("x64", "Win32")]
	[string]$Platform = "x64",

	[string]$Output = "$PSScriptRoot\coverage.xml",

	[ValidateRange(0, 100)]
	[double]$MinimumLineCoverage = 90,

	[ValidateRange(0, 100)]
	[double]$MinimumBlockCoverage = 95,

	[ValidateRange(0, 100)]
	[double]$MinimumSourceLineCoverage = 90,

	[ValidateRange(0, 100)]
	[double]$MinimumSourceBlockCoverage = 90
)

$ErrorActionPreference = "Stop"

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
	throw "Visual Studio Installer discovery tool not found: $vswhere"
}

$vsInstall = & $vswhere -latest -prerelease -products "*" -requires Microsoft.Component.MSBuild -property installationPath |
	Select-Object -First 1
if (-not $vsInstall) {
	throw "No Visual Studio installation with MSBuild was found."
}

$msbuild = Join-Path $vsInstall "MSBuild\Current\Bin\MSBuild.exe"
$collector = Join-Path $vsInstall "Common7\IDE\Extensions\Microsoft\CodeCoverage.Console\Microsoft.CodeCoverage.Console.exe"
$coverageProjectDirectory = Join-Path $PSScriptRoot "GeometricHashTable.Coverage"
$coverageProject = Join-Path $coverageProjectDirectory "GeometricHashTable.Coverage.vcxproj"
$coverageExecutable = Join-Path $coverageProjectDirectory "bin\$Platform\$Configuration\GeometricHashTable.Coverage.exe"

if (-not (Test-Path $msbuild)) {
	throw "MSBuild not found: $msbuild"
}

if (-not (Test-Path $collector)) {
	throw "Microsoft Code Coverage Console not found: $collector"
}

& $msbuild $coverageProject /t:Rebuild "/p:Configuration=$Configuration" "/p:Platform=$Platform" /m
if ($LASTEXITCODE -ne 0) {
	throw "Coverage project build failed."
}

if (-not (Test-Path $coverageExecutable)) {
	throw "Coverage executable not found: $coverageExecutable"
}

& $collector collect `
	--include-files $coverageExecutable `
	--output $Output `
	--output-format xml `
	--nologo `
	$coverageExecutable

if ($LASTEXITCODE -ne 0) {
	throw "Unit tests or code coverage collection failed."
}

if (-not (Test-Path $Output)) {
	throw "Coverage report was not created: $Output"
}

[xml]$coverageReport = Get-Content -LiteralPath $Output -Raw
$expectedSourcePaths = @(
	(Join-Path $PSScriptRoot "GeometricHashTable\HashTable.cpp"),
	(Join-Path $PSScriptRoot "GeometricHashTable\Hash.h"),
	(Join-Path $PSScriptRoot "GeometricHashTable\Triangle.h")
) | ForEach-Object { [System.IO.Path]::GetFullPath($_) }
$coveredSourcePaths = @{}
$coveredHashTableFunctions = @{}

$coverageTotals = [ordered]@{
	LinesCovered = [long]0
	LinesPartial = [long]0
	LinesMissed = [long]0
	BlocksCovered = [long]0
	BlocksMissed = [long]0
	Functions = [long]0
}
$sourceSummaries = @()

foreach ($module in @($coverageReport.results.modules.module)) {
	foreach ($sourceFile in @($module.source_files.source_file)) {
		$sourcePath = [System.IO.Path]::GetFullPath($sourceFile.GetAttribute("path"))
		if ($expectedSourcePaths -notcontains $sourcePath) {
			continue
		}

		$sourceId = $sourceFile.GetAttribute("id")
		$sourceFunctions = @()
		$seenFunctions = @{}
		foreach ($function in @($module.functions.function)) {
			$sourceRanges = @()
			foreach ($range in @($function.ranges.range)) {
				if ($range.GetAttribute("source_id") -eq $sourceId) {
					$sourceRanges += "{0}:{1}:{2}:{3}" -f `
						$range.GetAttribute("start_line"), $range.GetAttribute("end_line"), `
						$range.GetAttribute("start_column"), $range.GetAttribute("end_column")
				}
			}
			if ($sourceRanges.Count -gt 0) {
				$functionKey = "{0}|{1}|{2}|{3}" -f `
					$function.GetAttribute("name"), $function.GetAttribute("type_name"), `
					$function.GetAttribute("namespace"), ($sourceRanges -join ",")
				if (-not $seenFunctions.ContainsKey($functionKey)) {
					$sourceFunctions += $function
					$seenFunctions[$functionKey] = $true
				}
			}
		}

		if ($sourceFunctions.Count -eq 0) {
			continue
		}
		$coveredSourcePaths[$sourcePath] = $true
		if ($sourcePath -eq $expectedSourcePaths[0]) {
			foreach ($function in $sourceFunctions) {
				if ($function.GetAttribute("type_name") -eq "HashTable") {
					$coveredHashTableFunctions[$function.GetAttribute("name")] = $true
				}
			}
		}

		$fileTotals = [ordered]@{
			LinesCovered = [long]0
			LinesPartial = [long]0
			LinesMissed = [long]0
			BlocksCovered = [long]0
			BlocksMissed = [long]0
			Functions = [long]$sourceFunctions.Count
		}
		foreach ($function in $sourceFunctions) {
			$fileTotals.LinesCovered += [long]$function.GetAttribute("lines_covered")
			$fileTotals.LinesPartial += [long]$function.GetAttribute("lines_partially_covered")
			$fileTotals.LinesMissed += [long]$function.GetAttribute("lines_not_covered")
			$fileTotals.BlocksCovered += [long]$function.GetAttribute("blocks_covered")
			$fileTotals.BlocksMissed += [long]$function.GetAttribute("blocks_not_covered")
		}

		$sourceSummaries += [pscustomobject]@{
			Path = $sourcePath
			Functions = $sourceFunctions
			Totals = $fileTotals
		}
		$coverageTotals.LinesCovered += $fileTotals.LinesCovered
		$coverageTotals.LinesPartial += $fileTotals.LinesPartial
		$coverageTotals.LinesMissed += $fileTotals.LinesMissed
		$coverageTotals.BlocksCovered += $fileTotals.BlocksCovered
		$coverageTotals.BlocksMissed += $fileTotals.BlocksMissed
		$coverageTotals.Functions += $fileTotals.Functions
	}
}

if ($sourceSummaries.Count -eq 0) {
	throw "The coverage report contains no instrumented base-code source files."
}

$missingSourcePaths = @($expectedSourcePaths | Where-Object { -not $coveredSourcePaths.ContainsKey($_) })
if ($missingSourcePaths.Count -gt 0) {
	throw "Coverage report is missing expected base-code sources: $($missingSourcePaths -join ', ')"
}
$expectedHashTableFunctions = @("Construct", "_Initialize", "_ConstructTriangle", "Lookup")
$missingHashTableFunctions = @($expectedHashTableFunctions | Where-Object { -not $coveredHashTableFunctions.ContainsKey($_) })
if ($missingHashTableFunctions.Count -gt 0) {
	throw "Coverage report is missing HashTable functions: $($missingHashTableFunctions -join ', '). Use an unoptimized Debug coverage build."
}

function Get-CoveragePercent([long]$covered, [long]$partial, [long]$missed) {
	$total = $covered + $partial + $missed
	if ($total -eq 0) {
		return 0.0
	}
	return 100.0 * $covered / $total
}

function Get-BlockCoveragePercent([long]$covered, [long]$missed) {
	$total = $covered + $missed
	if ($total -eq 0) {
		return 0.0
	}
	return 100.0 * $covered / $total
}

Write-Host ""
Write-Host "Base code coverage summary"
$linePercent = Get-CoveragePercent $coverageTotals.LinesCovered $coverageTotals.LinesPartial $coverageTotals.LinesMissed
$blockPercent = Get-BlockCoveragePercent $coverageTotals.BlocksCovered $coverageTotals.BlocksMissed
Write-Host ("Overall: {0:N2}% lines ({1} covered, {2} partial, {3} missed); {4:N2}% blocks ({5} covered, {6} missed); {7} functions" -f `
	$linePercent, $coverageTotals.LinesCovered, $coverageTotals.LinesPartial, $coverageTotals.LinesMissed, `
	$blockPercent, $coverageTotals.BlocksCovered, $coverageTotals.BlocksMissed, $coverageTotals.Functions)
if ($linePercent -lt $MinimumLineCoverage) {
	throw "Line coverage $([math]::Round($linePercent, 2))% is below the required $MinimumLineCoverage%."
}
if ($blockPercent -lt $MinimumBlockCoverage) {
	throw "Block coverage $([math]::Round($blockPercent, 2))% is below the required $MinimumBlockCoverage%."
}

foreach ($sourceSummary in $sourceSummaries) {
	$totals = $sourceSummary.Totals
	$linePercent = Get-CoveragePercent $totals.LinesCovered $totals.LinesPartial $totals.LinesMissed
	$blockPercent = Get-BlockCoveragePercent $totals.BlocksCovered $totals.BlocksMissed
	Write-Host ""
	Write-Host ("File: {0}" -f $sourceSummary.Path)
	Write-Host ("  {0:N2}% lines ({1} covered, {2} partial, {3} missed); {4:N2}% blocks ({5} covered, {6} missed); {7} functions" -f `
		$linePercent, $totals.LinesCovered, $totals.LinesPartial, $totals.LinesMissed, `
		$blockPercent, $totals.BlocksCovered, $totals.BlocksMissed, $totals.Functions)
	if ($linePercent -lt $MinimumSourceLineCoverage) {
		throw "Source line coverage for $($sourceSummary.Path) is below the required $MinimumSourceLineCoverage%."
	}
	if ($blockPercent -lt $MinimumSourceBlockCoverage) {
		throw "Source block coverage for $($sourceSummary.Path) is below the required $MinimumSourceBlockCoverage%."
	}

	foreach ($function in $sourceSummary.Functions) {
		$functionLinesCovered = [long]$function.GetAttribute("lines_covered")
		$functionLinesPartial = [long]$function.GetAttribute("lines_partially_covered")
		$functionLinesMissed = [long]$function.GetAttribute("lines_not_covered")
		$functionBlocksCovered = [long]$function.GetAttribute("blocks_covered")
		$functionBlocksMissed = [long]$function.GetAttribute("blocks_not_covered")
		$functionLinePercent = Get-CoveragePercent $functionLinesCovered $functionLinesPartial $functionLinesMissed
		$functionBlockPercent = Get-BlockCoveragePercent $functionBlocksCovered $functionBlocksMissed
		$functionName = $function.GetAttribute("name")
		$typeName = $function.GetAttribute("type_name")
		if ($typeName) {
			$functionName = "$typeName::$functionName"
		}
		Write-Host ("    {0}: {1:N2}% lines ({2} covered, {3} partial, {4} missed); {5:N2}% blocks ({6} covered, {7} missed)" -f `
			$functionName, $functionLinePercent, $functionLinesCovered, $functionLinesPartial, $functionLinesMissed, `
			$functionBlockPercent, $functionBlocksCovered, $functionBlocksMissed)
	}
}

Write-Host ""
Write-Host "Detailed XML report: $Output"

$URL = "https://www.deltaconnected.com/arcdps/evtc/README.txt"
$OUTFILE = "src/Targets.h"

# Download file
$txt = Invoke-WebRequest -Uri $URL -UseBasicParsing | Select-Object -ExpandProperty Content

# Extract arrays
$primary = [regex]::Match($txt, "uint32_t\s+characters\[\]\s*=\s*\{([^}]*)\};", "Singleline").Groups[1].Value.Trim()
$secondary = [regex]::Match($txt, "uint32_t\s+characters_secondary\[\]\s*=\s*\{([^}]*)\};", "Singleline").Groups[1].Value.Trim()

if (-not $primary -or -not $secondary) {
	throw "Could not extract arrays from README.txt"
}

# Build header file
$header = @"
#pragma once
#include <vector>
#include <cstdint>

static std::vector<uint32_t> s_PrimaryTargets = {
$primary
};

static std::vector<uint32_t> s_SecondaryTargets = {
$secondary
};
"@

# Write output
Set-Content -Path $OUTFILE -Value $header -Encoding UTF8
Write-Host "Wrote $OUTFILE"

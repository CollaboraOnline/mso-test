# Call as `powershell.exe -File jenkins.ps1 -app word`
# app can be "word" or "excel" or "powerpoint"

param([Parameter(Mandatory=$true)][string]$app)
if ($app -ne "word" -and $app -ne "excel" -and $app -ne "powerpoint") {
	echo "Bad argument"
	exit 1
}

.\mso-test.exe "$app" | tee -filepath "output_$app.txt"
$failures = Select-String -Path "output_$app.txt" -Pattern "Fail:"

if ($failures -ne $null) {
	echo Failures:
	echo $failures
	exit 1
} else {
	echo No failures
	exit 0
}
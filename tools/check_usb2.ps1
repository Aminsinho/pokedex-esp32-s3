$devs = Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -like '*JTAG*' -or $_.Name -like '*USB Serial*' -or $_.Name -like '*UART*' }
foreach ($d in $devs) {
  Write-Output ('Name      : ' + $d.Name)
  Write-Output ('PNPClass  : ' + $d.PNPClass)
  Write-Output ('Status    : ' + $d.Status)
  Write-Output ('DeviceID  : ' + $d.DeviceID)
  Write-Output '---'
}
Write-Output ''
Write-Output '=== USB host controller detail (VID/PID) ==='
Get-CimInstance -Namespace root/standardcimv2 Win32_USBControllerDevice |
  ForEach-Object { $_.Dependent } | ForEach-Object {
    $c = Get-CimInstance Win32_PnPEntity -Filter "DeviceID='$($_.DeviceID)'"
    $d = Get-CimInstance Win32_USBControllerDevice -Filter "Dependent='$($_.DeviceID)'"
    Write-Output ($d.Dependent)
  }
Get-CimInstance -Namespace root/standardcimv2 Win32_USBHostController | Select-Object Name, DeviceID | Format-List

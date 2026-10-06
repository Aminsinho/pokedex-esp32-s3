Write-Output '=== USB-serial / Espressif devices ==='
Get-CimInstance Win32_PnPEntity |
  Where-Object { $_.Name -match 'USB-?SERIAL|CP210x|CH340|CH343|Silicon? Labs|Espressif|UART' } |
  Select-Object -ExpandProperty Name

Write-Output ''
Write-Output '=== COM ports ==='
Get-CimInstance Win32_SerialPort | Select-Object DeviceName, Description | Format-Table -AutoSize

Write-Output ''
Write-Output '=== All USB devices ==='
Get-CimInstance Win32_PnPEntity |
  Where-Object { $_.PNPClass -eq 'USB' -or $_.Name -match 'USB' } |
  Select-Object -ExpandProperty Name |
  Where-Object { $_ -match 'Composite|Serial|JTAG|Hub' } |
  Sort-Object -Unique

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Management;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace PokedexInstaller
{
    internal sealed class PortItem
    {
        public string Port;
        public string Name;
        public string Pnp;
        public bool IsEsp;
        public override string ToString() { return Port + " — " + Name; }
    }

    internal sealed class InstallerForm : Form
    {
        private readonly string root;
        private readonly ComboBox ports = new ComboBox();
        private readonly RadioButton kanto = new RadioButton();
        private readonly RadioButton full = new RadioButton();
        private readonly CheckBox narrations = new CheckBox();
        private readonly CheckBox cleanup = new CheckBox();
        private readonly TextBox ssid = new TextBox();
        private readonly TextBox password = new TextBox();
        private readonly TextBox backend = new TextBox();
        private readonly TextBox log = new TextBox();
        private readonly Label status = new Label();
        private readonly ProgressBar progress = new ProgressBar();
        private readonly Button install = new Button();
        private readonly Button refresh = new Button();
        private readonly Button camera = new Button();
        private Process child;

        public InstallerForm()
        {
            root = Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory));
            Text = "Instalador Pokédex ESP32-S3";
            ClientSize = new Size(760, 650);
            MinimumSize = new Size(776, 689);
            StartPosition = FormStartPosition.CenterScreen;
            Font = new Font("Segoe UI", 9F);
            BackColor = Color.FromArgb(241, 245, 249);

            Label title = new Label();
            title.Text = "POKÉDEX ESP32-S3";
            title.Font = new Font("Segoe UI Semibold", 22F, FontStyle.Bold);
            title.ForeColor = Color.FromArgb(25, 55, 78);
            title.SetBounds(28, 20, 430, 45);
            Controls.Add(title);

            Label intro = new Label();
            intro.Text = "Conecta la Pokédex por USB, introduce la microSD y completa estos datos.\r\nEl asistente instalará el firmware, los datos, la cámara del PC y la IA local.";
            intro.ForeColor = Color.FromArgb(71, 85, 105);
            intro.SetBounds(31, 68, 690, 42);
            Controls.Add(intro);

            GroupBox deviceBox = Box("1. Dispositivo", 28, 116, 704, 86);
            ports.DropDownStyle = ComboBoxStyle.DropDownList;
            ports.SetBounds(18, 32, 535, 28);
            deviceBox.Controls.Add(ports);
            refresh.Text = "Buscar";
            refresh.SetBounds(566, 30, 112, 31);
            refresh.Click += delegate { RefreshPorts(); };
            deviceBox.Controls.Add(refresh);

            GroupBox dataBox = Box("2. Contenido", 28, 212, 704, 112);
            kanto.Text = "Kanto — 151 Pokémon (más rápido)";
            kanto.SetBounds(18, 27, 300, 25);
            dataBox.Controls.Add(kanto);
            full.Text = "Completo — 1025 Pokémon";
            full.Checked = true;
            full.SetBounds(350, 27, 280, 25);
            dataBox.Controls.Add(full);
            narrations.Text = "Instalar narraciones españolas (descarga adicional de 413 MiB)";
            narrations.SetBounds(18, 57, 520, 25);
            dataBox.Controls.Add(narrations);
            cleanup.Text = "Liberar archivos temporales al finalizar";
            cleanup.Checked = true;
            cleanup.SetBounds(18, 82, 360, 25);
            dataBox.Controls.Add(cleanup);

            GroupBox networkBox = Box("3. Red local", 28, 334, 704, 132);
            AddField(networkBox, "Wi-Fi (SSID)", ssid, 18, 28, false);
            AddField(networkBox, "Contraseña", password, 18, 72, true);
            AddField(networkBox, "IPv4 de este PC", backend, 365, 28, false);
            Label hint = new Label();
            hint.Text = "La Pokédex y el PC deben estar conectados a la misma red.";
            hint.ForeColor = Color.FromArgb(100, 116, 139);
            hint.SetBounds(365, 77, 310, 35);
            networkBox.Controls.Add(hint);

            install.Text = "INSTALAR POKÉDEX";
            install.Font = new Font("Segoe UI Semibold", 11F, FontStyle.Bold);
            install.BackColor = Color.FromArgb(190, 35, 55);
            install.ForeColor = Color.White;
            install.FlatStyle = FlatStyle.Flat;
            install.SetBounds(28, 480, 245, 44);
            install.Click += async delegate { await RunInstall(); };
            Controls.Add(install);

            camera.Text = "ESP32-CAM experimental";
            camera.SetBounds(285, 480, 190, 44);
            camera.Enabled = false;
            camera.Click += delegate { StartCameraInstaller(); };
            Controls.Add(camera);

            status.Text = "Esperando una placa ESP32…";
            status.SetBounds(490, 487, 240, 30);
            status.TextAlign = ContentAlignment.MiddleRight;
            Controls.Add(status);

            progress.SetBounds(28, 532, 704, 12);
            progress.Style = ProgressBarStyle.Continuous;
            Controls.Add(progress);

            log.Multiline = true;
            log.ReadOnly = true;
            log.ScrollBars = ScrollBars.Vertical;
            log.BackColor = Color.FromArgb(20, 30, 40);
            log.ForeColor = Color.FromArgb(220, 238, 245);
            log.Font = new Font("Consolas", 8.5F);
            log.SetBounds(28, 555, 704, 72);
            Controls.Add(log);

            backend.Text = SuggestedIPv4();
            RefreshPorts();
            FormClosing += OnClosing;
        }

        private GroupBox Box(string text, int x, int y, int w, int h)
        {
            GroupBox box = new GroupBox();
            box.Text = text;
            box.SetBounds(x, y, w, h);
            box.BackColor = Color.White;
            Controls.Add(box);
            return box;
        }

        private static void AddField(Control parent, string label, TextBox field, int x, int y, bool secret)
        {
            Label caption = new Label();
            caption.Text = label;
            caption.SetBounds(x, y, 125, 24);
            parent.Controls.Add(caption);
            field.SetBounds(x + 125, y - 2, 205, 27);
            field.UseSystemPasswordChar = secret;
            parent.Controls.Add(field);
        }

        private void RefreshPorts()
        {
            ports.Items.Clear();
            foreach (PortItem item in ReadPorts()) ports.Items.Add(item);
            if (ports.Items.Count > 0)
            {
                int selected = 0;
                for (int i = 0; i < ports.Items.Count; i++)
                    if (((PortItem)ports.Items[i]).IsEsp) { selected = i; break; }
                ports.SelectedIndex = selected;
                status.Text = ((PortItem)ports.SelectedItem).IsEsp ? "ESP32 detectada" : "Revisa el puerto seleccionado";
            }
            else status.Text = "Conecta la Pokédex y pulsa Buscar";
        }

        private static List<PortItem> ReadPorts()
        {
            List<PortItem> result = new List<PortItem>();
            try
            {
                using (ManagementObjectSearcher searcher = new ManagementObjectSearcher("SELECT DeviceID,Name,PNPDeviceID FROM Win32_SerialPort"))
                foreach (ManagementObject value in searcher.Get())
                {
                    string port = Convert.ToString(value["DeviceID"]);
                    if (String.Equals(port, "COM1", StringComparison.OrdinalIgnoreCase)) continue;
                    string name = Convert.ToString(value["Name"]);
                    string pnp = Convert.ToString(value["PNPDeviceID"]);
                    string identity = (name + " " + pnp).ToUpperInvariant();
                    bool esp = identity.Contains("VID_303A") || identity.Contains("VID_1A86") ||
                               identity.Contains("VID_10C4") || identity.Contains("ESP32") ||
                               identity.Contains("CP210") || identity.Contains("CH340");
                    result.Add(new PortItem { Port = port, Name = name, Pnp = pnp, IsEsp = esp });
                }
            }
            catch { }
            return result;
        }

        private static string SuggestedIPv4()
        {
            try
            {
                foreach (System.Net.IPAddress address in Dns.GetHostAddresses(Dns.GetHostName()))
                    if (address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork && !IPAddress.IsLoopback(address))
                        return address.ToString();
            }
            catch { }
            return "";
        }

        private bool ValidateInput(out PortItem port)
        {
            port = ports.SelectedItem as PortItem;
            if (port == null) { MessageBox.Show("Conecta la Pokédex y pulsa Buscar.", "Falta la placa", MessageBoxButtons.OK, MessageBoxIcon.Warning); return false; }
            if (!port.IsEsp && MessageBox.Show("El puerto no parece pertenecer a una ESP32. ¿Continuar?", "Comprobar puerto", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return false;
            if (String.IsNullOrWhiteSpace(ssid.Text)) { MessageBox.Show("Escribe el nombre de la red Wi-Fi."); return false; }
            IPAddress parsed;
            if (!IPAddress.TryParse(backend.Text.Trim(), out parsed)) { MessageBox.Show("La IPv4 del PC no es válida."); return false; }
            return true;
        }

        private void WriteWifi()
        {
            string path = Path.Combine(root, "src", "pokedex", "wifi_config.h");
            string content = "#pragma once\r\n\r\n// Generado localmente por el instalador gráfico. No subir a Git.\r\n" +
                "#define WIFI_SSID      \"" + EscapeC(ssid.Text) + "\"\r\n" +
                "#define WIFI_PASSWORD  \"" + EscapeC(password.Text) + "\"\r\n" +
                "#define BACKEND_HOST   \"" + EscapeC(backend.Text.Trim()) + "\"\r\n" +
                "#define BACKEND_PORT   8000\r\n";
            File.WriteAllText(path, content, new UTF8Encoding(false));
        }

        private static string EscapeC(string value) { return value.Replace("\\", "\\\\").Replace("\"", "\\\""); }

        private async Task RunInstall()
        {
            PortItem port;
            if (!ValidateInput(out port)) return;
            WriteWifi();
            SetBusy(true);
            Append("Iniciando instalación guiada…");
            string script = Path.Combine(root, "tools", "instalar_pokedex.ps1");
            string args = "-NoProfile -ExecutionPolicy Bypass -File " + Q(script) + " -Port " + Q(port.Port) +
                (kanto.Checked ? " -Kanto" : " -Full") + " -ReuseWifi" +
                (narrations.Checked ? " -InstallNarrations" : " -SkipNarrations") +
                (cleanup.Checked ? " -Cleanup" : "");
            int code = await RunProcess("powershell.exe", args);
            SetBusy(false);
            if (code == 0)
            {
                status.Text = "Instalación completada";
                progress.Style = ProgressBarStyle.Continuous;
                progress.Value = 100;
                camera.Enabled = true;
                MessageBox.Show("La Pokédex está instalada. Usa el acceso directo 'Iniciar Pokedex' del escritorio.\r\n\r\nSi también tienes una ESP32-CAM, pulsa su botón después de conectarla.", "Instalación completada", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            else
            {
                status.Text = "La instalación necesita atención";
                MessageBox.Show("La instalación se detuvo. Revisa las últimas líneas del registro y vuelve a intentarlo.", "No se completó", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private Task<int> RunProcess(string file, string args)
        {
            return Task.Run(delegate
            {
                ProcessStartInfo info = new ProcessStartInfo(file, args);
                info.WorkingDirectory = root;
                info.UseShellExecute = false;
                info.CreateNoWindow = true;
                info.RedirectStandardOutput = true;
                info.RedirectStandardError = true;
                child = new Process();
                child.StartInfo = info;
                child.OutputDataReceived += delegate(object sender, DataReceivedEventArgs e) { if (e.Data != null) Append(e.Data); };
                child.ErrorDataReceived += delegate(object sender, DataReceivedEventArgs e) { if (e.Data != null) Append(e.Data); };
                child.Start();
                child.BeginOutputReadLine();
                child.BeginErrorReadLine();
                child.WaitForExit();
                return child.ExitCode;
            });
        }

        private void Append(string text)
        {
            if (InvokeRequired) { BeginInvoke(new Action<string>(Append), text); return; }
            log.AppendText(text + Environment.NewLine);
            log.SelectionStart = log.TextLength;
            log.ScrollToCaret();
        }

        private void SetBusy(bool busy)
        {
            install.Enabled = !busy;
            refresh.Enabled = !busy;
            ports.Enabled = !busy;
            progress.Style = busy ? ProgressBarStyle.Marquee : ProgressBarStyle.Continuous;
            if (busy) status.Text = "Instalando… no desconectes la placa";
        }

        private void StartCameraInstaller()
        {
            string batch = Path.Combine(root, "INSTALAR ESP32-CAM EXPERIMENTAL.bat");
            Process.Start(new ProcessStartInfo(batch) { WorkingDirectory = root, UseShellExecute = true });
        }

        private void OnClosing(object sender, FormClosingEventArgs e)
        {
            if (child != null && !child.HasExited)
            {
                if (MessageBox.Show("La instalación sigue en curso. ¿Quieres cancelarla?", "Cancelar instalación", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) { e.Cancel = true; return; }
                try { child.Kill(); } catch { }
            }
        }

        private static string Q(string value) { return "\"" + value.Replace("\"", "\\\"") + "\""; }
    }

    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            if (args.Length > 0 && String.Equals(args[0], "--self-test", StringComparison.OrdinalIgnoreCase))
            {
                string root = AppDomain.CurrentDomain.BaseDirectory;
                string[] required = {
                    Path.Combine(root, "tools", "instalar_pokedex.ps1"),
                    Path.Combine(root, "src", "pokedex", "pokedex.ino"),
                    Path.Combine(root, "backend", "requirements-camera.txt")
                };
                foreach (string path in required)
                    if (!File.Exists(path)) { Environment.ExitCode = 2; return; }
                using (InstallerForm form = new InstallerForm()) { }
                Environment.ExitCode = 0;
                return;
            }
            Application.Run(new InstallerForm());
        }
    }
}

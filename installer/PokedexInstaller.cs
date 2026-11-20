// Asistente de instalacion de la Pokedex ESP32-S3.
// Interfaz guiada paso a paso; el trabajo real lo hace tools/instalar_pokedex.ps1.
// Compilar con installer/build_installer_exe.ps1 (C# 5, .NET Framework 4.x incluido en Windows).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Globalization;
using System.IO;
using System.Management;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace PokedexInstaller
{
    internal sealed class PortItem
    {
        public string Port;
        public string Name;
        public bool IsEsp;
        public bool IsEspS3;
        public override string ToString()
        {
            if (IsEspS3) return "Pokédex (ESP32-S3) en " + Port;
            return Name + (Name.Contains(Port) ? "" : " (" + Port + ")");
        }
    }

    internal sealed class Stage
    {
        public string Key;
        public string Title;
        public int Weight;
        public string State = "pending"; // pending | running | done | failed
        public Label Icon;
        public Label Text;
    }

    internal sealed class WizardForm : Form
    {
        // ---- Estilo ---------------------------------------------------------
        private static readonly Color Red = Color.FromArgb(204, 31, 52);
        private static readonly Color RedDark = Color.FromArgb(160, 22, 40);
        private static readonly Color Ink = Color.FromArgb(30, 41, 59);
        private static readonly Color Muted = Color.FromArgb(100, 116, 139);
        private static readonly Color Border = Color.FromArgb(226, 232, 240);
        private static readonly Color Soft = Color.FromArgb(248, 250, 252);
        private static readonly Color Good = Color.FromArgb(22, 163, 74);
        private static readonly Color Warn = Color.FromArgb(217, 119, 6);
        private static readonly Color Bad = Color.FromArgb(220, 38, 38);

        private const int W = 760, H = 580, HeaderH = 86, FooterH = 66;
        private const int PWelcome = 0, PConnect = 1, PWifi = 2, PContent = 3, PInstall = 4, PDone = 5, PError = 6;
        private static readonly string[] PageTitles = {
            "Bienvenida", "Conecta la Pokédex", "Red Wi-Fi", "¿Qué quieres instalar?",
            "Instalando", "¡Listo!", "La instalación se ha detenido"
        };

        private readonly string root;
        private readonly Label headerStep = new Label();
        private readonly Label headerTitle = new Label();
        private readonly Panel bodyHost = new Panel();
        private readonly Button back = new Button();
        private readonly Button next = new Button();
        private readonly List<Panel> pages = new List<Panel>();
        private int current = -1;

        // Bienvenida
        private readonly Label diskLabel = new Label();

        // Conexion
        private readonly Label connectIcon = new Label();
        private readonly Label connectTitle = new Label();
        private readonly Label connectDetail = new Label();
        private readonly ComboBox ports = new ComboBox();
        private readonly Timer portTimer = new Timer();
        private string portSignature = "";

        // Wi-Fi
        private readonly ComboBox ssid = new ComboBox();
        private readonly TextBox password = new TextBox();
        private readonly CheckBox showPassword = new CheckBox();
        private readonly LinkLabel advancedLink = new LinkLabel();
        private readonly Label ipCaption = new Label();
        private readonly TextBox ip = new TextBox();
        private readonly Label ipHint = new Label();

        // Contenido
        private readonly RadioButton full = new RadioButton();
        private readonly RadioButton kanto = new RadioButton();
        private readonly CheckBox voices = new CheckBox();
        private readonly CheckBox cleanup = new CheckBox();

        // Instalacion
        private readonly Panel stageHost = new Panel();
        private readonly Panel overall = new Panel();
        private readonly Label nowLabel = new Label();
        private readonly Label elapsedLabel = new Label();
        private readonly LinkLabel toggleLog = new LinkLabel();
        private readonly TextBox log = new TextBox();
        private readonly Timer clock = new Timer();
        private readonly List<Stage> stages = new List<Stage>();
        private Stage running;
        private double stageFraction;
        private int shownProgress;
        private DateTime startedAt;
        private int lastId = 1025;
        private string lastError;
        private readonly StringBuilder fullLog = new StringBuilder();
        private StreamWriter logFile;
        private Process child;
        private bool installing;

        // Final / error
        private readonly Label errorProblem = new Label();
        private readonly Label errorHint = new Label();
        private readonly Label errorDetail = new Label();

        public bool AutoMode;
        public int AutoExitCode = -1;

        [DllImport("user32.dll")]
        private static extern bool SetProcessDPIAware();
        public static void EnableDpi() { try { SetProcessDPIAware(); } catch { } }

        public WizardForm()
        {
            root = Path.GetFullPath(AppDomain.CurrentDomain.BaseDirectory);
            AutoScaleDimensions = new SizeF(96F, 96F);
            AutoScaleMode = AutoScaleMode.Dpi;
            Text = "Instalador de la Pokédex";
            ClientSize = new Size(W, H);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            Font = new Font("Segoe UI", 10F);
            BackColor = Color.White;
            ForeColor = Ink;

            BuildChrome();
            pages.Add(BuildWelcome());
            pages.Add(BuildConnect());
            pages.Add(BuildWifi());
            pages.Add(BuildContent());
            pages.Add(BuildInstall());
            pages.Add(BuildDone());
            pages.Add(BuildError());
            foreach (Panel p in pages) { p.Visible = false; bodyHost.Controls.Add(p); }

            portTimer.Interval = 1500;
            portTimer.Tick += delegate { RefreshPorts(false); };
            clock.Interval = 1000;
            clock.Tick += delegate { UpdateElapsed(); };
            FormClosing += OnClosing;

            PrefillWifi();
            ShowPage(PWelcome);
        }

        // =====================================================================
        // Estructura comun: cabecera, cuerpo y botonera
        // =====================================================================
        private void BuildChrome()
        {
            Panel header = new Panel();
            header.SetBounds(0, 0, W, HeaderH);
            header.BackColor = Red;
            header.Paint += PaintHeader;
            Controls.Add(header);

            headerStep.SetBounds(96, 16, 600, 22);
            headerStep.ForeColor = Color.FromArgb(255, 220, 225);
            headerStep.BackColor = Color.Transparent;
            headerStep.Font = new Font("Segoe UI", 9.5F);
            header.Controls.Add(headerStep);

            headerTitle.SetBounds(94, 38, 640, 36);
            headerTitle.ForeColor = Color.White;
            headerTitle.BackColor = Color.Transparent;
            headerTitle.Font = new Font("Segoe UI Semibold", 17F, FontStyle.Bold);
            header.Controls.Add(headerTitle);

            bodyHost.SetBounds(0, HeaderH, W, H - HeaderH - FooterH);
            bodyHost.BackColor = Color.White;
            Controls.Add(bodyHost);

            Panel footer = new Panel();
            footer.SetBounds(0, H - FooterH, W, FooterH);
            footer.BackColor = Soft;
            footer.Paint += delegate(object s, PaintEventArgs e) { using (Pen pen = new Pen(Border)) e.Graphics.DrawLine(pen, 0, 0, W, 0); };
            Controls.Add(footer);

            StyleSecondary(back);
            back.Text = "Atrás";
            back.SetBounds(W - 330, 14, 140, 38);
            back.Click += delegate { OnBack(); };
            footer.Controls.Add(back);

            StylePrimary(next);
            next.SetBounds(W - 178, 14, 150, 38);
            next.Click += delegate { OnNext(); };
            footer.Controls.Add(next);
            AcceptButton = next;
        }

        private static void PaintHeader(object sender, PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            Rectangle r = new Rectangle(28, 18, 50, 50);
            using (SolidBrush white = new SolidBrush(Color.White)) g.FillEllipse(white, r);
            using (SolidBrush dark = new SolidBrush(RedDark)) g.FillPie(dark, r, 180, 180);
            using (Pen pen = new Pen(Color.FromArgb(40, 20, 25), 3F))
            {
                g.DrawEllipse(pen, r);
                g.DrawLine(pen, r.Left, r.Top + 25, r.Right, r.Top + 25);
                g.FillEllipse(Brushes.White, r.Left + 17, r.Top + 17, 16, 16);
                g.DrawEllipse(pen, r.Left + 17, r.Top + 17, 16, 16);
            }
        }

        private static void StylePrimary(Button b)
        {
            b.FlatStyle = FlatStyle.Flat;
            b.FlatAppearance.BorderSize = 0;
            b.BackColor = Red;
            b.ForeColor = Color.White;
            b.Font = new Font("Segoe UI Semibold", 10.5F, FontStyle.Bold);
            b.Cursor = Cursors.Hand;
            // Desactivado debe verse desactivado (el estilo plano no lo hace solo).
            b.EnabledChanged += delegate
            {
                b.BackColor = b.Enabled ? Red : Color.FromArgb(203, 213, 225);
                b.Cursor = b.Enabled ? Cursors.Hand : Cursors.Default;
            };
        }

        private static void StyleSecondary(Button b)
        {
            b.FlatStyle = FlatStyle.Flat;
            b.FlatAppearance.BorderColor = Border;
            b.BackColor = Color.White;
            b.ForeColor = Ink;
            b.Cursor = Cursors.Hand;
        }

        private static Panel NewPage()
        {
            Panel p = new Panel();
            p.SetBounds(0, 0, W, H - HeaderH - FooterH);
            p.BackColor = Color.White;
            return p;
        }

        private static Label AddLabel(Control parent, string text, int x, int y, int w, int h, float size, bool bold, Color color)
        {
            Label l = new Label();
            l.Text = text;
            l.SetBounds(x, y, w, h);
            l.ForeColor = color;
            l.Font = new Font(bold ? "Segoe UI Semibold" : "Segoe UI", size, bold ? FontStyle.Bold : FontStyle.Regular);
            l.UseMnemonic = false;
            parent.Controls.Add(l);
            return l;
        }

        // =====================================================================
        // Paginas
        // =====================================================================
        private Panel BuildWelcome()
        {
            Panel p = NewPage();
            AddLabel(p, "Este asistente instala todo lo necesario para que tu Pokédex funcione: el programa de la placa, los datos de la microSD y la inteligencia artificial que reconoce Pokémon con la cámara del PC.",
                40, 28, 680, 66, 11F, false, Ink);
            AddLabel(p, "Antes de empezar, ten a mano:", 40, 106, 680, 26, 11F, true, Ink);
            string[] items = {
                "La Pokédex con su tarjeta microSD dentro",
                "Un cable USB de datos (algunos cables solo cargan)",
                "Conexión a Internet: se descargan varios GB la primera vez",
                "Tiempo: puede tardar más de una hora; no hace falta estar delante"
            };
            for (int i = 0; i < items.Length; i++)
            {
                AddLabel(p, "✔", 52, 140 + i * 34, 24, 26, 11F, true, Good);
                AddLabel(p, items[i], 80, 140 + i * 34, 640, 26, 11F, false, Ink);
            }
            diskLabel.SetBounds(40, 300, 680, 50);
            diskLabel.Font = new Font("Segoe UI", 10F);
            diskLabel.UseMnemonic = false;
            p.Controls.Add(diskLabel);
            UpdateDiskSpace();
            AddLabel(p, "Pulsa «Empezar» cuando lo tengas todo.", 40, 368, 680, 26, 10F, false, Muted);
            return p;
        }

        private Panel BuildConnect()
        {
            Panel p = NewPage();
            AddLabel(p, "Conecta la Pokédex al PC con el cable USB. La buscaremos automáticamente.",
                40, 26, 680, 26, 11F, false, Ink);

            Panel card = new Panel();
            card.SetBounds(40, 70, 680, 120);
            card.BackColor = Soft;
            card.Paint += delegate(object s, PaintEventArgs e) { using (Pen pen = new Pen(Border)) e.Graphics.DrawRectangle(pen, 0, 0, card.Width - 1, card.Height - 1); };
            p.Controls.Add(card);

            connectIcon.SetBounds(20, 22, 70, 70);
            connectIcon.Font = new Font("Segoe UI Symbol", 34F);
            connectIcon.TextAlign = ContentAlignment.MiddleCenter;
            card.Controls.Add(connectIcon);
            connectTitle.SetBounds(100, 26, 560, 30);
            connectTitle.Font = new Font("Segoe UI Semibold", 13F, FontStyle.Bold);
            connectTitle.UseMnemonic = false;
            card.Controls.Add(connectTitle);
            connectDetail.SetBounds(100, 58, 560, 44);
            connectDetail.ForeColor = Muted;
            connectDetail.UseMnemonic = false;
            card.Controls.Add(connectDetail);

            AddLabel(p, "Puerto USB:", 40, 214, 110, 26, 10F, false, Muted);
            ports.DropDownStyle = ComboBoxStyle.DropDownList;
            ports.SetBounds(150, 210, 430, 30);
            ports.SelectedIndexChanged += delegate { UpdateConnectState(); };
            p.Controls.Add(ports);
            Button refresh = new Button();
            StyleSecondary(refresh);
            refresh.Text = "Buscar de nuevo";
            refresh.SetBounds(592, 208, 128, 34);
            refresh.Click += delegate { RefreshPorts(true); };
            p.Controls.Add(refresh);

            AddLabel(p, "¿No aparece?", 40, 266, 680, 24, 10.5F, true, Ink);
            AddLabel(p, "• Usa otro cable: muchos cables de carga no transmiten datos.\r\n" +
                        "• Prueba otro puerto USB del PC, mejor uno trasero o sin hub.\r\n" +
                        "• Cierra Arduino IDE u otros programas que puedan estar usando la placa.",
                40, 292, 680, 80, 10F, false, Muted);
            return p;
        }

        private Panel BuildWifi()
        {
            Panel p = NewPage();
            AddLabel(p, "La Pokédex usa el Wi-Fi para enviar las fotos al PC y reconocer el Pokémon. Escribe los datos de la red de tu casa.",
                40, 24, 680, 48, 11F, false, Ink);

            AddLabel(p, "Nombre de la red (SSID)", 40, 88, 300, 24, 10F, true, Ink);
            ssid.DropDownStyle = ComboBoxStyle.DropDown;
            ssid.SetBounds(40, 114, 420, 30);
            p.Controls.Add(ssid);

            AddLabel(p, "Contraseña del Wi-Fi", 40, 158, 300, 24, 10F, true, Ink);
            password.SetBounds(40, 184, 420, 30);
            password.UseSystemPasswordChar = true;
            p.Controls.Add(password);
            showPassword.Text = "Mostrar";
            showPassword.SetBounds(474, 186, 120, 26);
            showPassword.CheckedChanged += delegate { password.UseSystemPasswordChar = !showPassword.Checked; };
            p.Controls.Add(showPassword);

            Panel note = new Panel();
            note.SetBounds(40, 232, 680, 64);
            note.BackColor = Color.FromArgb(255, 247, 237);
            p.Controls.Add(note);
            AddLabel(note, "⚠  La Pokédex solo se conecta a redes de 2,4 GHz. Si tu router separa las redes, elige la que no termina en «5G». El PC y la Pokédex deben estar en la misma red.",
                12, 8, 656, 50, 9.5F, false, Color.FromArgb(124, 45, 18));

            advancedLink.Text = "Opciones avanzadas";
            advancedLink.SetBounds(40, 310, 200, 24);
            advancedLink.LinkColor = Muted;
            advancedLink.LinkClicked += delegate { SetAdvanced(!ip.Visible); };
            p.Controls.Add(advancedLink);
            ipCaption.Text = "IP de este PC";
            ipCaption.SetBounds(40, 342, 140, 24);
            p.Controls.Add(ipCaption);
            ip.SetBounds(180, 338, 200, 30);
            p.Controls.Add(ip);
            ipHint.Text = "Detectada automáticamente. Cámbiala solo si sabes que no es correcta.";
            ipHint.SetBounds(392, 336, 330, 40);
            ipHint.ForeColor = Muted;
            ipHint.Font = new Font("Segoe UI", 9F);
            p.Controls.Add(ipHint);
            ip.Text = SuggestedIPv4();
            SetAdvanced(false);
            return p;
        }

        private void SetAdvanced(bool visible)
        {
            ipCaption.Visible = ip.Visible = ipHint.Visible = visible;
            advancedLink.Text = visible ? "Ocultar opciones avanzadas" : "Opciones avanzadas";
        }

        private Panel BuildContent()
        {
            Panel p = NewPage();
            AddLabel(p, "Elige cuántos Pokémon quieres en la microSD.", 40, 24, 680, 26, 11F, false, Ink);

            Panel fullCard = OptionCard(p, full, 40, 62, "Todos los Pokémon (1025)  — recomendado",
                "Las nueve generaciones completas. La copia a la microSD tarda bastante más.");
            Panel kantoCard = OptionCard(p, kanto, 40, 146, "Solo Kanto (151)",
                "La primera generación. Es lo más rápido; puedes instalar el resto más tarde.");
            full.Checked = true;
            full.CheckedChanged += delegate { fullCard.Invalidate(); kantoCard.Invalidate(); };

            voices.Text = "Añadir las voces en español de cada Pokémon (descarga extra de 413 MB)";
            voices.SetBounds(40, 246, 680, 28);
            p.Controls.Add(voices);
            cleanup.Text = "Borrar los archivos temporales al terminar (recomendado)";
            cleanup.Checked = true;
            cleanup.SetBounds(40, 280, 680, 28);
            p.Controls.Add(cleanup);

            AddLabel(p, "Al pulsar «Instalar» empezará la instalación. No desconectes la Pokédex hasta que termine.",
                40, 332, 680, 44, 10F, false, Muted);
            return p;
        }

        private Panel OptionCard(Panel parent, RadioButton radio, int x, int y, string title, string detail)
        {
            Panel card = new Panel();
            card.SetBounds(x, y, 680, 72);
            card.BackColor = Color.White;
            card.Cursor = Cursors.Hand;
            card.Paint += delegate(object s, PaintEventArgs e)
            {
                Color c = radio.Checked ? Red : Border;
                using (Pen pen = new Pen(c, radio.Checked ? 2F : 1F))
                    e.Graphics.DrawRectangle(pen, 1, 1, card.Width - 3, card.Height - 3);
            };
            radio.Text = title;
            radio.Font = new Font("Segoe UI Semibold", 11F, FontStyle.Bold);
            radio.SetBounds(16, 10, 640, 28);
            radio.CheckedChanged += delegate { card.Invalidate(); };
            card.Controls.Add(radio);
            Label d = AddLabel(card, detail, 38, 38, 620, 24, 9.5F, false, Muted);
            EventHandler pick = delegate { radio.Checked = true; };
            card.Click += pick;
            d.Click += pick;
            parent.Controls.Add(card);
            return card;
        }

        private Panel BuildInstall()
        {
            Panel p = NewPage();
            stageHost.SetBounds(40, 18, 680, 236);
            p.Controls.Add(stageHost);

            overall.SetBounds(40, 262, 680, 14);
            overall.BackColor = Color.FromArgb(241, 245, 249);
            overall.Paint += delegate(object s, PaintEventArgs e)
            {
                int width = (int)(overall.Width * shownProgress / 1000.0);
                using (SolidBrush fill = new SolidBrush(Red)) e.Graphics.FillRectangle(fill, 0, 0, width, overall.Height);
                using (Pen pen = new Pen(Border)) e.Graphics.DrawRectangle(pen, 0, 0, overall.Width - 1, overall.Height - 1);
            };
            p.Controls.Add(overall);

            nowLabel.SetBounds(40, 284, 520, 24);
            nowLabel.ForeColor = Muted;
            nowLabel.UseMnemonic = false;
            nowLabel.AutoEllipsis = true;
            p.Controls.Add(nowLabel);
            elapsedLabel.SetBounds(560, 284, 160, 24);
            elapsedLabel.ForeColor = Muted;
            elapsedLabel.TextAlign = ContentAlignment.TopRight;
            p.Controls.Add(elapsedLabel);

            toggleLog.Text = "Ver detalles técnicos";
            toggleLog.SetBounds(40, 312, 200, 22);
            toggleLog.LinkColor = Muted;
            toggleLog.LinkClicked += delegate
            {
                log.Visible = !log.Visible;
                toggleLog.Text = log.Visible ? "Ocultar detalles técnicos" : "Ver detalles técnicos";
                if (log.Visible) { log.SelectionStart = log.TextLength; log.ScrollToCaret(); }
            };
            p.Controls.Add(toggleLog);

            log.Multiline = true;
            log.ReadOnly = true;
            log.ScrollBars = ScrollBars.Vertical;
            log.BackColor = Color.FromArgb(20, 30, 40);
            log.ForeColor = Color.FromArgb(220, 238, 245);
            log.Font = new Font("Consolas", 8.5F);
            log.SetBounds(40, 336, 680, 90);
            log.Visible = false;
            p.Controls.Add(log);
            return p;
        }

        private Panel BuildDone()
        {
            Panel p = NewPage();
            AddLabel(p, "✔", 40, 24, 56, 44, 24F, true, Good);
            AddLabel(p, "Tu Pokédex está instalada.", 104, 30, 600, 34, 15F, true, Ink);
            AddLabel(p, "Para empezar a usarla:", 40, 98, 680, 26, 11F, true, Ink);
            string[] steps = {
                "Desconecta la Pokédex del USB y vuelve a conectarla para que cargue los datos nuevos.",
                "Para escanear Pokémon, abre «Iniciar Pokédex» (hay un acceso directo en el escritorio). Pon la webcam del PC delante del Pokémon.",
                "Mantén el PC encendido y en la misma red Wi-Fi mientras uses la función de escaneo."
            };
            for (int i = 0; i < steps.Length; i++)
            {
                AddLabel(p, (i + 1).ToString(CultureInfo.InvariantCulture), 44, 132 + i * 56, 26, 26, 11F, true, Red);
                AddLabel(p, steps[i], 72, 132 + i * 56, 640, 50, 10.5F, false, Ink);
            }
            Button launch = new Button();
            StylePrimary(launch);
            launch.Text = "Iniciar Pokédex ahora";
            launch.SetBounds(40, 314, 220, 40);
            launch.Click += delegate { LaunchBat("Iniciar Pokedex - PCCam.bat"); };
            p.Controls.Add(launch);

            LinkLabel cam = new LinkLabel();
            cam.Text = "¿Tienes una ESP32-CAM? Instalarla (experimental)";
            cam.SetBounds(40, 370, 400, 24);
            cam.LinkColor = Muted;
            cam.LinkClicked += delegate { LaunchBat("INSTALAR ESP32-CAM EXPERIMENTAL.bat"); };
            p.Controls.Add(cam);
            return p;
        }

        private Panel BuildError()
        {
            Panel p = NewPage();
            AddLabel(p, "!", 40, 18, 60, 60, 32F, true, Bad);
            errorProblem.SetBounds(104, 28, 616, 60);
            errorProblem.Font = new Font("Segoe UI Semibold", 13F, FontStyle.Bold);
            errorProblem.UseMnemonic = false;
            p.Controls.Add(errorProblem);

            AddLabel(p, "Qué puedes hacer:", 40, 100, 680, 24, 11F, true, Ink);
            errorHint.SetBounds(40, 126, 680, 80);
            errorHint.Font = new Font("Segoe UI", 10.5F);
            errorHint.UseMnemonic = false;
            p.Controls.Add(errorHint);

            AddLabel(p, "Mensaje técnico:", 40, 214, 680, 22, 9.5F, true, Muted);
            errorDetail.SetBounds(40, 238, 680, 62);
            errorDetail.Font = new Font("Consolas", 9F);
            errorDetail.ForeColor = Muted;
            errorDetail.UseMnemonic = false;
            errorDetail.AutoEllipsis = true;
            p.Controls.Add(errorDetail);

            Button copy = new Button();
            StyleSecondary(copy);
            copy.Text = "Copiar detalles";
            copy.SetBounds(40, 316, 160, 36);
            copy.Click += delegate
            {
                try { Clipboard.SetText(TailLog(120)); MessageBox.Show(this, "Copiado. Pégalo en un mensaje al autor del proyecto.", "Detalles copiados"); }
                catch { }
            };
            p.Controls.Add(copy);
            Button open = new Button();
            StyleSecondary(open);
            open.Text = "Abrir registro completo";
            open.SetBounds(212, 316, 200, 36);
            open.Click += delegate
            {
                string path = GuiLogPath();
                if (File.Exists(path)) Process.Start(new ProcessStartInfo("notepad.exe", Q(path)));
            };
            p.Controls.Add(open);
            AddLabel(p, "Reintentar es seguro: lo que ya se descargó o instaló no se vuelve a descargar.",
                40, 368, 680, 24, 9.5F, false, Muted);
            return p;
        }

        // =====================================================================
        // Navegacion
        // =====================================================================
        internal void ShowPage(int index)
        {
            if (current >= 0) pages[current].Visible = false;
            current = index;
            pages[index].Visible = true;
            headerTitle.Text = PageTitles[index];
            headerStep.Text = index <= PInstall ? "Paso " + (index + 1) + " de 5" : "Instalador de la Pokédex";

            back.Visible = index >= PConnect && index <= PContent || index == PError;
            back.Text = index == PError ? "Cambiar opciones" : "Atrás";
            back.SetBounds(index == PError ? W - 360 : W - 330, 14, index == PError ? 170 : 140, 38);
            next.Enabled = true;
            switch (index)
            {
                case PWelcome: next.Text = "Empezar"; break;
                case PContent: next.Text = "Instalar"; break;
                case PInstall: next.Text = "Instalando…"; next.Enabled = false; break;
                case PDone: next.Text = "Cerrar"; break;
                case PError: next.Text = "Reintentar"; break;
                default: next.Text = "Siguiente"; break;
            }
            if (index == PConnect) { RefreshPorts(true); portTimer.Start(); }
            else portTimer.Stop();
            if (index == PWifi) ssid.Focus();
        }

        private void OnBack()
        {
            if (current == PError) ShowPage(PConnect);
            else if (current > PWelcome && current <= PContent) ShowPage(current - 1);
        }

        private void OnNext()
        {
            switch (current)
            {
                case PWelcome: ShowPage(PConnect); break;
                case PConnect: if (ValidatePort()) ShowPage(PWifi); break;
                case PWifi: if (ValidateWifi()) ShowPage(PContent); break;
                case PContent: StartInstall(); break;
                case PDone: Close(); break;
                case PError: StartInstall(); break;
            }
        }

        // =====================================================================
        // Comprobaciones y deteccion
        // =====================================================================
        private void UpdateDiskSpace()
        {
            try
            {
                long free = Math.Min(new DriveInfo(Path.GetPathRoot(root)).AvailableFreeSpace,
                    new DriveInfo(Path.GetPathRoot(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData))).AvailableFreeSpace);
                double gb = free / 1073741824.0;
                if (gb >= 15)
                {
                    diskLabel.Text = "✔  Espacio libre en el disco: " + gb.ToString("0", CultureInfo.InvariantCulture) + " GB (suficiente).";
                    diskLabel.ForeColor = Good;
                }
                else
                {
                    diskLabel.Text = "⚠  Solo quedan " + gb.ToString("0.0", CultureInfo.InvariantCulture) + " GB libres. Se recomiendan al menos 15 GB; libera espacio antes de continuar.";
                    diskLabel.ForeColor = Warn;
                }
            }
            catch { diskLabel.Text = ""; }
        }

        private void RefreshPorts(bool force)
        {
            List<PortItem> found = ReadPorts();
            StringBuilder sig = new StringBuilder();
            foreach (PortItem item in found) sig.Append(item.Port).Append('|');
            if (!force && sig.ToString() == portSignature) return;
            portSignature = sig.ToString();

            string previous = ports.SelectedItem is PortItem ? ((PortItem)ports.SelectedItem).Port : null;
            ports.Items.Clear();
            found.Sort(delegate(PortItem a, PortItem b)
            {
                int ra = a.IsEspS3 ? 0 : a.IsEsp ? 1 : 2, rb = b.IsEspS3 ? 0 : b.IsEsp ? 1 : 2;
                return ra != rb ? ra.CompareTo(rb) : string.Compare(a.Port, b.Port, StringComparison.Ordinal);
            });
            foreach (PortItem item in found) ports.Items.Add(item);
            int select = ports.Items.Count > 0 ? 0 : -1;
            for (int i = 0; i < ports.Items.Count; i++)
                if (((PortItem)ports.Items[i]).Port == previous) select = i;
            ports.SelectedIndex = select;
            UpdateConnectState();
        }

        private void UpdateConnectState()
        {
            PortItem item = ports.SelectedItem as PortItem;
            if (item == null)
            {
                connectIcon.Text = "⌛";
                connectIcon.ForeColor = Muted;
                connectTitle.Text = "Esperando a la Pokédex…";
                connectTitle.ForeColor = Ink;
                connectDetail.Text = "Conéctala ahora con el cable USB. Esta pantalla se actualiza sola.";
                if (current == PConnect) next.Enabled = false;
            }
            else if (item.IsEsp)
            {
                connectIcon.Text = "✔";
                connectIcon.ForeColor = Good;
                connectTitle.Text = "¡Pokédex encontrada!";
                connectTitle.ForeColor = Good;
                connectDetail.Text = "Conectada en " + item.Port + ". Pulsa «Siguiente» para continuar.";
                if (current == PConnect) next.Enabled = true;
            }
            else
            {
                connectIcon.Text = "?";
                connectIcon.ForeColor = Warn;
                connectTitle.Text = "Hay un dispositivo, pero no parece la Pokédex";
                connectTitle.ForeColor = Warn;
                connectDetail.Text = item.Name + ". Si estás seguro de que es la Pokédex, puedes continuar.";
                if (current == PConnect) next.Enabled = true;
            }
        }

        private bool ValidatePort()
        {
            PortItem item = ports.SelectedItem as PortItem;
            if (item == null) return false;
            if (!item.IsEsp)
                return MessageBox.Show(this, "El puerto " + item.Port + " no parece ser la Pokédex.\r\n\r\n¿Continuar de todas formas?",
                    "Comprobar puerto", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes;
            return true;
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
                    string id = (name + " " + Convert.ToString(value["PNPDeviceID"])).ToUpperInvariant();
                    PortItem item = new PortItem();
                    item.Port = port;
                    item.Name = name;
                    item.IsEspS3 = id.Contains("VID_303A");
                    item.IsEsp = item.IsEspS3 || id.Contains("VID_1A86") || id.Contains("VID_10C4") ||
                                 id.Contains("ESP32") || id.Contains("CP210") || id.Contains("CH340");
                    result.Add(item);
                }
            }
            catch { }
            return result;
        }

        private static string SuggestedIPv4()
        {
            string best = "";
            int bestRank = int.MaxValue;
            try
            {
                foreach (NetworkInterface nic in NetworkInterface.GetAllNetworkInterfaces())
                {
                    if (nic.OperationalStatus != OperationalStatus.Up) continue;
                    if (nic.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;
                    string label = (nic.Name + " " + nic.Description).ToLowerInvariant();
                    if (label.Contains("vethernet") || label.Contains("virtual") || label.Contains("vmware") ||
                        label.Contains("hyper-v") || label.Contains("vpn") || label.Contains("tap-") || label.Contains("wsl")) continue;
                    IPInterfaceProperties props = nic.GetIPProperties();
                    bool gateway = false;
                    foreach (GatewayIPAddressInformation g in props.GatewayAddresses)
                        if (g.Address.AddressFamily == AddressFamily.InterNetwork && !g.Address.Equals(IPAddress.Any)) gateway = true;
                    if (!gateway) continue;
                    foreach (UnicastIPAddressInformation a in props.UnicastAddresses)
                    {
                        if (a.Address.AddressFamily != AddressFamily.InterNetwork) continue;
                        string s = a.Address.ToString();
                        if (s.StartsWith("169.254.")) continue;
                        int rank = nic.NetworkInterfaceType == NetworkInterfaceType.Wireless80211 ? 0 : 1;
                        if (rank < bestRank) { best = s; bestRank = rank; }
                    }
                }
            }
            catch { }
            return best;
        }

        private static string RunQuiet(string file, string args)
        {
            try
            {
                ProcessStartInfo info = new ProcessStartInfo(file, args);
                info.UseShellExecute = false;
                info.CreateNoWindow = true;
                info.RedirectStandardOutput = true;
                info.StandardOutputEncoding = Encoding.GetEncoding(CultureInfo.CurrentCulture.TextInfo.OEMCodePage);
                using (Process p = Process.Start(info))
                {
                    string output = p.StandardOutput.ReadToEnd();
                    p.WaitForExit(4000);
                    return output;
                }
            }
            catch { return ""; }
        }

        private void PrefillWifi()
        {
            List<string> names = new List<string>();
            string connected = null;
            foreach (string line in RunQuiet("netsh", "wlan show interfaces").Split('\n'))
            {
                Match m = Regex.Match(line, @"^\s*SSID\s*:\s*(.+?)\s*$");
                if (m.Success) { connected = m.Groups[1].Value; names.Add(connected); }
            }
            foreach (string line in RunQuiet("netsh", "wlan show profiles").Split('\n'))
            {
                Match m = Regex.Match(line, @"^\s{2,}[^:]+?\s:\s(.+?)\s*$");
                if (m.Success && !names.Contains(m.Groups[1].Value)) names.Add(m.Groups[1].Value);
            }
            foreach (string n in names) ssid.Items.Add(n);
            if (connected != null) ssid.Text = connected;

            // Reinstalacion: reutiliza la configuracion anterior si existe.
            string config = Path.Combine(root, "src", "pokedex", "wifi_config.h");
            if (File.Exists(config))
            {
                try
                {
                    string text = File.ReadAllText(config);
                    string s = ReadDefine(text, "WIFI_SSID"), pw = ReadDefine(text, "WIFI_PASSWORD"), host = ReadDefine(text, "BACKEND_HOST");
                    // Ignora los valores de ejemplo de wifi_config.example.h.
                    if (!String.IsNullOrEmpty(s) && s != "NOMBRE_DE_TU_WIFI") { ssid.Text = s; password.Text = pw ?? ""; }
                    IPAddress parsed;
                    if (!String.IsNullOrEmpty(host) && host != "192.168.1.100" && IPAddress.TryParse(host, out parsed) && String.IsNullOrEmpty(ip.Text)) ip.Text = host;
                }
                catch { }
            }
        }

        private static string ReadDefine(string text, string name)
        {
            Match m = Regex.Match(text, @"#define\s+" + name + @"\s+""((?:[^""\\]|\\.)*)""");
            if (!m.Success) return null;
            return Regex.Replace(m.Groups[1].Value, @"\\(.)", "$1");
        }

        private bool ValidateWifi()
        {
            if (String.IsNullOrWhiteSpace(ssid.Text))
            {
                MessageBox.Show(this, "Escribe el nombre de tu red Wi-Fi.", "Falta la red", MessageBoxButtons.OK, MessageBoxIcon.Information);
                ssid.Focus();
                return false;
            }
            if (password.Text.Length == 0 &&
                MessageBox.Show(this, "No has escrito contraseña. ¿Tu Wi-Fi es una red abierta sin contraseña?", "Contraseña vacía",
                    MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes)
            {
                password.Focus();
                return false;
            }
            IPAddress parsed;
            if (!IPAddress.TryParse(ip.Text.Trim(), out parsed) || parsed.AddressFamily != AddressFamily.InterNetwork)
            {
                SetAdvanced(true);
                MessageBox.Show(this, "No se pudo detectar la IP de este PC. Escríbela en «IP de este PC» (por ejemplo 192.168.1.20).",
                    "Falta la IP", MessageBoxButtons.OK, MessageBoxIcon.Information);
                ip.Focus();
                return false;
            }
            return true;
        }

        private void WriteWifi()
        {
            string path = Path.Combine(root, "src", "pokedex", "wifi_config.h");
            string content = "#pragma once\r\n\r\n// Generado localmente por el instalador. No subir a Git.\r\n" +
                "#define WIFI_SSID      \"" + EscapeC(ssid.Text.Trim()) + "\"\r\n" +
                "#define WIFI_PASSWORD  \"" + EscapeC(password.Text) + "\"\r\n" +
                "#define BACKEND_HOST   \"" + EscapeC(ip.Text.Trim()) + "\"\r\n" +
                "#define BACKEND_PORT   8000\r\n";
            File.WriteAllText(path, content, new UTF8Encoding(false));
        }

        private static string EscapeC(string value) { return value.Replace("\\", "\\\\").Replace("\"", "\\\""); }

        // =====================================================================
        // Instalacion
        // =====================================================================
        internal void BuildStages(bool withVoices)
        {
            stages.Clear();
            stageHost.Controls.Clear();
            AddStage("tools", "Descargar las herramientas de Arduino y ESP32", 25);
            AddStage("python", "Preparar Python", 10);
            AddStage("firmware", "Instalar el programa en la Pokédex", 20);
            AddStage("data", "Preparar los datos de los Pokémon", 12);
            AddStage("sd", "Copiar los datos a la microSD", 20);
            if (withVoices) AddStage("voices", "Copiar las voces en español", 10);
            AddStage("ai", "Preparar la inteligencia artificial", 8);
            int rowH = Math.Min(34, 236 / stages.Count);
            for (int i = 0; i < stages.Count; i++)
            {
                Stage s = stages[i];
                s.Icon = AddLabel(stageHost, "○", 0, i * rowH, 30, rowH, 12F, false, Muted);
                s.Text = AddLabel(stageHost, s.Title, 32, i * rowH + 3, 640, rowH - 2, 10.5F, false, Muted);
            }
            running = null;
            stageFraction = 0;
            shownProgress = 0;
            overall.Invalidate();
            nowLabel.Text = "Comenzando…";
        }

        private void AddStage(string key, string title, int weight)
        {
            Stage s = new Stage();
            s.Key = key;
            s.Title = title;
            s.Weight = weight;
            stages.Add(s);
        }

        private void PaintStage(Stage s)
        {
            switch (s.State)
            {
                case "done": s.Icon.Text = "✔"; s.Icon.ForeColor = Good; s.Text.ForeColor = Ink; s.Text.Font = new Font("Segoe UI", 10.5F); break;
                case "running": s.Icon.Text = "●"; s.Icon.ForeColor = Red; s.Text.ForeColor = Ink; s.Text.Font = new Font("Segoe UI Semibold", 10.5F, FontStyle.Bold); break;
                case "failed": s.Icon.Text = "✖"; s.Icon.ForeColor = Bad; s.Text.ForeColor = Bad; s.Text.Font = new Font("Segoe UI Semibold", 10.5F, FontStyle.Bold); break;
                default: s.Icon.Text = "○"; s.Icon.ForeColor = Muted; s.Text.ForeColor = Muted; s.Text.Font = new Font("Segoe UI", 10.5F); break;
            }
        }

        private void EnterStage(string key)
        {
            int index = stages.FindIndex(delegate(Stage s) { return s.Key == key; });
            if (index < 0) return;
            for (int i = 0; i < stages.Count; i++)
            {
                string state = i < index ? "done" : i == index ? "running" : stages[i].State == "done" ? "done" : "pending";
                if (stages[i].State != state) { stages[i].State = state; PaintStage(stages[i]); }
            }
            if (running != stages[index]) stageFraction = 0;
            running = stages[index];
            UpdateProgress();
        }

        private void UpdateProgress()
        {
            double total = 0, done = 0;
            foreach (Stage s in stages)
            {
                total += s.Weight;
                if (s.State == "done") done += s.Weight;
                else if (s == running) done += s.Weight * Math.Max(0, Math.Min(1, stageFraction));
            }
            int value = total > 0 ? (int)(1000 * done / total) : 0;
            if (value > shownProgress) shownProgress = Math.Min(1000, value);
            overall.Invalidate();
        }

        private void UpdateElapsed()
        {
            TimeSpan t = DateTime.Now - startedAt;
            elapsedLabel.Text = "Tiempo: " + ((int)t.TotalMinutes).ToString(CultureInfo.InvariantCulture) + " min " + t.Seconds.ToString("00", CultureInfo.InvariantCulture) + " s";
        }

        internal void HandleLine(string raw)
        {
            if (raw == null) return;
            string[] parts = raw.Split('\r');
            string first = parts[0];
            string last = "";
            for (int i = parts.Length - 1; i >= 0; i--) if (parts[i].Trim().Length > 0) { last = parts[i]; break; }
            AppendLog(last.Length > 0 && parts.Length > 1 ? first.Substring(0, Math.Min(first.Length, 120)) + " … " + last.Trim() : raw);

            Match m = Regex.Match(first.Trim(), @"^== (.+) ==$");
            if (m.Success) { EnterSection(m.Groups[1].Value); return; }
            m = Regex.Match(first, @"^  (\S[^\r\n]{2,90}?)\.\.\.");
            if (m.Success) nowLabel.Text = "Ahora: " + m.Groups[1].Value;
            if (first.StartsWith("ERROR: ")) lastError = first.Substring(7).Trim();

            string probe = last.Length > 0 ? last : first;
            m = Regex.Match(probe, @"(\d+)/(\d+) verified");
            if (m.Success) { stageFraction = Ratio(m.Groups[1].Value, m.Groups[2].Value); nowLabel.Text = "Copiando a la microSD: " + m.Groups[1].Value + " de " + m.Groups[2].Value; UpdateProgress(); return; }
            m = Regex.Match(probe, @"^ID (\d+): (\d+) sprites");
            if (m.Success) { stageFraction = Ratio(m.Groups[1].Value, lastId.ToString(CultureInfo.InvariantCulture)); nowLabel.Text = "Copiando imágenes: Pokémon " + m.Groups[1].Value + " de " + lastId; UpdateProgress(); return; }
            m = Regex.Match(probe, @"(\d{1,3}(?:\.\d+)?)\s?%");
            if (m.Success)
            {
                double pct;
                if (double.TryParse(m.Groups[1].Value, NumberStyles.Float, CultureInfo.InvariantCulture, out pct) && pct <= 100)
                { stageFraction = Math.Max(stageFraction, pct / 100.0 * 0.9); UpdateProgress(); }
            }
        }

        private static double Ratio(string a, string b)
        {
            double x = double.Parse(a, CultureInfo.InvariantCulture), y = double.Parse(b, CultureInfo.InvariantCulture);
            return y > 0 ? x / y : 0;
        }

        private void EnterSection(string section)
        {
            string s = section.ToLowerInvariant();
            if (s.Contains("arduino")) EnterStage("tools");
            else if (s.Contains("python")) EnterStage("python");
            else if (s.Contains("firmware")) EnterStage("firmware");
            else if (s.Contains("preparando datos")) EnterStage("data");
            else if (s.Contains("copiando datos")) EnterStage("sd");
            else if (s.Contains("narraciones")) EnterStage("voices");
            else if (s.Contains("inteligencia")) EnterStage("ai");
            else if (s.Contains("completada"))
            {
                foreach (Stage st in stages) { st.State = "done"; PaintStage(st); }
                shownProgress = 1000; overall.Invalidate();
            }
            if (s.Contains("buscando")) nowLabel.Text = "Buscando la Pokédex por USB…";
            if (s.Contains("liberando")) nowLabel.Text = "Borrando archivos temporales…";
        }

        private void AppendLog(string text)
        {
            fullLog.AppendLine(text);
            if (fullLog.Length > 400000) fullLog.Remove(0, fullLog.Length - 300000);
            if (logFile != null) { try { logFile.WriteLine(text); } catch { } }
            if (log.TextLength > 200000) log.Text = log.Text.Substring(log.TextLength - 120000);
            log.AppendText(text + Environment.NewLine);
        }

        private string TailLog(int lines)
        {
            string[] all = fullLog.ToString().Split(new[] { Environment.NewLine }, StringSplitOptions.None);
            int start = Math.Max(0, all.Length - lines);
            return "Error: " + (lastError ?? "(sin mensaje)") + Environment.NewLine + "----" + Environment.NewLine +
                   String.Join(Environment.NewLine, all, start, all.Length - start);
        }

        private string GuiLogPath() { return Path.Combine(root, ".installer", "instalador-gui.log"); }

        internal async void StartInstall()
        {
            PortItem port = ports.SelectedItem as PortItem;
            if (port == null) { ShowPage(PConnect); return; }
            try { WriteWifi(); }
            catch (Exception e)
            {
                MessageBox.Show(this, "No se pudo guardar la configuración Wi-Fi:\r\n" + e.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            lastId = kanto.Checked ? 151 : 1025;
            lastError = null;
            fullLog.Clear();
            log.Clear();
            BuildStages(voices.Checked);
            ShowPage(PInstall);
            startedAt = DateTime.Now;
            UpdateElapsed();
            clock.Start();
            installing = true;
            try
            {
                Directory.CreateDirectory(Path.Combine(root, ".installer"));
                logFile = new StreamWriter(GuiLogPath(), false, new UTF8Encoding(false));
                logFile.AutoFlush = true;
            }
            catch { logFile = null; }

            string script = Path.Combine(root, "tools", "instalar_pokedex.ps1");
            string args = "-NoProfile -ExecutionPolicy Bypass -File " + Q(script) + " -Port " + Q(port.Port) +
                (kanto.Checked ? " -Kanto" : " -Full") + " -ReuseWifi" +
                (voices.Checked ? " -InstallNarrations" : " -SkipNarrations") +
                (cleanup.Checked ? " -Cleanup" : "");
            AppendLog("> powershell " + args);
            int code;
            try { code = await RunProcess("powershell.exe", args); }
            catch (Exception e) { lastError = e.Message; code = -1; }
            installing = false;
            clock.Stop();
            if (logFile != null) { try { logFile.Dispose(); } catch { } logFile = null; }
            Finish(code);
        }

        private void Finish(int code)
        {
            if (code == 0)
            {
                foreach (Stage s in stages) { s.State = "done"; PaintStage(s); }
                shownProgress = 1000; overall.Invalidate();
                ShowPage(PDone);
            }
            else
            {
                if (running != null) { running.State = "failed"; PaintStage(running); }
                ExplainError();
                ShowPage(PError);
            }
            if (AutoMode) { AutoExitCode = code; Close(); }
        }

        internal void ExplainError()
        {
            string err = lastError ?? "La instalación terminó de forma inesperada.";
            string e = err.ToLowerInvariant();
            string key = running != null ? running.Key : "";
            string problem, hint;
            if (e.Contains("no se detecto") || e.Contains("no reaparecio") || e.Contains("could not open port") ||
                e.Contains("permissionerror") || e.Contains("acceso denegado") || e.Contains("access is denied"))
            {
                problem = "No se pudo comunicar con la Pokédex por USB.";
                hint = "Desconecta la Pokédex, espera unos segundos y vuelve a conectarla. Cierra Arduino IDE u otros programas que la estén usando y pulsa «Reintentar».";
            }
            else if (key == "sd" || key == "voices")
            {
                problem = "No se pudieron copiar los datos a la microSD.";
                hint = "Comprueba que la tarjeta microSD está bien metida en la Pokédex y formateada en FAT32 o exFAT. Desconecta y vuelve a conectar la Pokédex y pulsa «Reintentar».";
            }
            else if (e.Contains("cargar firmware"))
            {
                problem = "No se pudo instalar el programa en la Pokédex.";
                hint = "Desconecta la Pokédex. Mantén pulsado el botón BOOT de la placa mientras la vuelves a conectar, suéltalo y pulsa «Reintentar».";
            }
            else if (e.Contains("python"))
            {
                problem = "Hubo un problema con Python.";
                hint = "Instala Python 3.12 desde python.org (marca «Add python.exe to PATH»), cierra este instalador, ábrelo de nuevo y pulsa «Reintentar».";
            }
            else if (key == "tools" || e.Contains("descarg") || e.Contains("conexi") || e.Contains("connect") || e.Contains("remote name") || e.Contains("resolver"))
            {
                problem = "Falló una descarga.";
                hint = "Comprueba la conexión a Internet y pulsa «Reintentar». Si usas una red del trabajo o del colegio, puede que bloquee las descargas: prueba con otra red.";
            }
            else if (key == "ai")
            {
                problem = "No se pudo preparar la inteligencia artificial (Ollama).";
                hint = "La Pokédex ya tiene sus datos. Reinicia el PC y pulsa «Reintentar»; si sigue fallando, instala Ollama desde ollama.com.";
            }
            else if (key == "firmware")
            {
                problem = "No se pudo compilar el programa de la Pokédex.";
                hint = "Pulsa «Reintentar». Si vuelve a fallar, usa «Copiar detalles» y envíaselos al autor del proyecto.";
            }
            else
            {
                problem = "La instalación se detuvo.";
                hint = "Pulsa «Reintentar». Si vuelve a fallar, usa «Copiar detalles» y envíaselos al autor del proyecto.";
            }
            errorProblem.Text = problem;
            errorHint.Text = hint;
            errorDetail.Text = err;
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
                info.RedirectStandardInput = true;
                info.StandardOutputEncoding = Encoding.UTF8;
                info.StandardErrorEncoding = Encoding.UTF8;
                info.EnvironmentVariables["PYTHONIOENCODING"] = "utf-8";
                info.EnvironmentVariables["PYTHONUNBUFFERED"] = "1";
                child = new Process();
                child.StartInfo = info;
                child.OutputDataReceived += delegate(object s, DataReceivedEventArgs e) { if (e.Data != null) Post(e.Data); };
                child.ErrorDataReceived += delegate(object s, DataReceivedEventArgs e) { if (e.Data != null) Post(e.Data); };
                child.Start();
                child.StandardInput.Close(); // cualquier pregunta inesperada recibe "vacio" en vez de colgarse
                child.BeginOutputReadLine();
                child.BeginErrorReadLine();
                child.WaitForExit();
                child.WaitForExit(); // vacia los eventos de salida pendientes
                return child.ExitCode;
            });
        }

        private void Post(string line)
        {
            if (IsDisposed) return;
            try { BeginInvoke(new Action<string>(HandleLine), line); } catch { }
        }

        private void LaunchBat(string name)
        {
            string path = Path.Combine(root, name);
            if (!File.Exists(path)) { MessageBox.Show(this, "No se encontró " + name, "Falta un archivo"); return; }
            Process.Start(new ProcessStartInfo(path) { WorkingDirectory = root, UseShellExecute = true });
        }

        private void OnClosing(object sender, FormClosingEventArgs e)
        {
            if (!installing || child == null || child.HasExited || AutoMode) return;
            if (MessageBox.Show(this, "La instalación sigue en marcha. Si la cancelas ahora tendrás que repetirla.\r\n\r\n¿Cancelar la instalación?",
                    "Cancelar instalación", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes)
            {
                e.Cancel = true;
                return;
            }
            try
            {
                // Termina tambien los procesos hijos (arduino-cli, python...) para liberar el puerto USB.
                Process.Start(new ProcessStartInfo("taskkill", "/T /F /PID " + child.Id) { CreateNoWindow = true, UseShellExecute = false }).WaitForExit(5000);
            }
            catch { }
        }

        private static string Q(string value) { return "\"" + value.Replace("\"", "\\\"") + "\""; }

        // ---- Utilidades para pruebas automaticas ----------------------------
        internal bool SelectPortForTest(string port)
        {
            RefreshPorts(true);
            for (int i = 0; i < ports.Items.Count; i++)
                if (String.Equals(((PortItem)ports.Items[i]).Port, port, StringComparison.OrdinalIgnoreCase)) { ports.SelectedIndex = i; return true; }
            return false;
        }
        internal void ChooseKantoForTest() { kanto.Checked = true; voices.Checked = false; cleanup.Checked = false; }
        internal void SetErrorForTest(string stageKey, string error)
        {
            EnterStage(stageKey);
            lastError = error;
            running.State = "failed";
            PaintStage(running);
            ExplainError();
        }
    }

    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            WizardForm.EnableDpi();
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            string mode = args.Length > 0 ? args[0].ToLowerInvariant() : "";

            if (mode == "--self-test")
            {
                string root = AppDomain.CurrentDomain.BaseDirectory;
                string[] required = {
                    Path.Combine(root, "tools", "instalar_pokedex.ps1"),
                    Path.Combine(root, "src", "pokedex", "pokedex.ino"),
                    Path.Combine(root, "backend", "requirements-camera.txt")
                };
                foreach (string path in required)
                    if (!File.Exists(path)) { Environment.ExitCode = 2; return; }
                using (WizardForm form = new WizardForm()) { }
                Environment.ExitCode = 0;
                return;
            }

            if (mode == "--capture" && args.Length > 1)
            {
                // Genera capturas de cada pantalla (para revisar el diseno sin hacer clic).
                Directory.CreateDirectory(args[1]);
                using (WizardForm form = new WizardForm())
                {
                    form.StartPosition = FormStartPosition.Manual;
                    form.Location = new Point(40, 40);
                    form.Show();
                    for (int page = 0; page < 4; page++) Snap(form, page, args[1], "pagina" + page);
                    form.BuildStages(true);
                    form.ShowPage(4);
                    foreach (string l in new[] {
                        "== Preparando Arduino y ESP32 ==", "  Instalar soporte ESP32 3.3.11...Descargando paquetes...",
                        "== Preparando Python ==", "  Instalar dependencias Python... OK",
                        "== Compilando e instalando el firmware ==", "  Cargar firmware en COM4...Writing at 0x0005801a [===>   ]  64.5% 131072/901381 bytes...",
                        "== Preparando datos para la microSD ==", "== Copiando datos a la microSD de la Pokedex ==",
                        "  Copiar catalogo y fichas...100/1026 verified", "400/1026 verified" })
                        form.HandleLine(l);
                    Snap(form, 4, args[1], "pagina4-instalando");
                    form.SetErrorForTest("sd", "Copiar catalogo y fichas fallo con codigo 1.");
                    Snap(form, 6, args[1], "pagina6-error");
                    Snap(form, 5, args[1], "pagina5-listo");
                    form.Close();
                }
                return;
            }

            if (mode == "--auto" && args.Length > 1)
            {
                // Instalacion desatendida para pruebas: --auto COM4 (Kanto, sin voces, Wi-Fi de wifi_config.h).
                WizardForm form = new WizardForm();
                form.AutoMode = true;
                form.Shown += delegate
                {
                    if (!form.SelectPortForTest(args[1])) { form.AutoExitCode = 3; form.Close(); return; }
                    form.ChooseKantoForTest();
                    form.StartInstall();
                };
                Application.Run(form);
                Environment.ExitCode = form.AutoExitCode;
                return;
            }

            Application.Run(new WizardForm());
        }

        private static void Snap(WizardForm form, int page, string dir, string name)
        {
            form.ShowPage(page);
            Application.DoEvents();
            using (Bitmap bmp = new Bitmap(form.Width, form.Height))
            {
                form.DrawToBitmap(bmp, new Rectangle(0, 0, form.Width, form.Height));
                bmp.Save(Path.Combine(dir, name + ".png"), System.Drawing.Imaging.ImageFormat.Png);
            }
        }
    }
}

// Plot.cs — 給 C# 用的 QtPlot 包裝。
//
//     using var plot = new QtPlot.Plot();
//     var s = plot.AddScatter(xs, ys);
//     s.Legend = "量測值";
//     plot.Title = "溫度";
//     plot.SavePng("out.png", 800, 600);
//
// 錯誤一律丟 QtPlotException（訊息來自 qp_last_error）。
// 一個 Plot 只能在一條執行緒上用 —— 通常就是 UI 執行緒。
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

namespace QtPlot
{
    public enum AxisEdge { Bottom = 0, Left = 1, Top = 2, Right = 3 }
    public enum PlotTheme { Light = 0, Dark = 1, Hardcopy = 2 }
    public enum Corner { TopLeft = 0, TopRight = 1, BottomLeft = 2, BottomRight = 3 }
    public enum Colormap { Viridis = 0, Turbo, Jet, Grayscale, Blues, Hot, Coolwarm }

    public enum MarkerShape
    {
        None = 0,
        FilledCircle, OpenCircle,
        FilledSquare, OpenSquare,
        FilledTriangleUp, OpenTriangleUp,
        FilledTriangleDown, OpenTriangleDown,
        FilledDiamond, OpenDiamond,
        Plus, Asterisk, HashTag, Eks,
        VerticalBar, HorizontalBar
    }

    public sealed class QtPlotException : Exception
    {
        public int Code { get; }
        public QtPlotException(int code, string message) : base(message) { Code = code; }
    }

    /// <summary>座標軸範圍。</summary>
    public readonly struct AxisLimits
    {
        public double XMin { get; }
        public double XMax { get; }
        public double YMin { get; }
        public double YMax { get; }
        public AxisLimits(double xMin, double xMax, double yMin, double yMax)
        {
            XMin = xMin; XMax = xMax; YMin = yMin; YMax = yMax;
        }
        public override string ToString() => $"X [{XMin:G6}, {XMax:G6}]  Y [{YMin:G6}, {YMax:G6}]";
    }

    public sealed class Plot : IDisposable
    {
        private IntPtr _handle;

        public Plot()
        {
            _handle = Native.qp_plot_create();
            if (_handle == IntPtr.Zero) throw new QtPlotException(-6, "qp_plot_create 失敗：" + LastError());
        }

        ~Plot() { Destroy(); }

        public void Dispose()
        {
            Destroy();
            GC.SuppressFinalize(this);
        }

        private void Destroy()
        {
            if (_handle == IntPtr.Zero) return;
            Native.qp_plot_destroy(_handle);
            _handle = IntPtr.Zero;
        }

        internal IntPtr Handle
        {
            get
            {
                if (_handle == IntPtr.Zero) throw new ObjectDisposedException(nameof(Plot));
                return _handle;
            }
        }

        public static string Version => Marshal.PtrToStringAnsi(Native.qp_version()) ?? "";

        /// <summary>
        /// 換 QtPlot 介面文字的語言（"en"、"ja"、"de"……；null 代表跟著系統）。影響所有的圖，
        /// 已經建好的圖下次 Render 時就會換。有載入翻譯回傳 true。
        /// </summary>
        public static bool SetLanguage(string? code) => Check(Native.qp_set_language(Utf8(code))) == 1;

        /// <summary>可以選的語言代碼（含原始語言 zh_TW）。</summary>
        public static string[] AvailableLanguages =>
            Utf8Ptr(Native.qp_available_languages()).Split(',', StringSplitOptions.RemoveEmptyEntries);

        // ───────────── 外觀 ─────────────

        private string _title = "";
        public string Title
        {
            get => _title;
            set { Check(Native.qp_set_title(Handle, Utf8(value))); _title = value ?? ""; }
        }

        public void SetAxisLabel(AxisEdge axis, string text) => Check(Native.qp_set_axis_label(Handle, (int)axis, Utf8(text)));
        public void SetTheme(PlotTheme theme) => Check(Native.qp_set_style(Handle, (int)theme));
        public void ShowLegend(Corner corner = Corner.TopRight) => Check(Native.qp_set_legend(Handle, 1, (int)corner));
        public void HideLegend() => Check(Native.qp_set_legend(Handle, 0, (int)Corner.TopRight));
        public void SetGrid(bool visible) => Check(Native.qp_set_grid(Handle, visible ? 1 : 0));
        /// <summary>抗鋸齒。資料量非常大時關掉會比較快。</summary>
        public void SetAntiAlias(bool enabled) => Check(Native.qp_set_antialias(Handle, enabled ? 1 : 0));
        /// <summary>換字型家族，並把所有字級乘上 scale（維持標題比刻度大的層次）。</summary>
        public void SetFont(string? family, double scale = 0) => Check(Native.qp_set_font(Handle, Utf8(family), scale));

        // ───────────── 視野 ─────────────

        public AxisLimits Limits
        {
            get
            {
                Check(Native.qp_get_limits(Handle, out var x0, out var x1, out var y0, out var y1));
                return new AxisLimits(x0, x1, y0, y1);
            }
            set => Check(Native.qp_set_limits(Handle, value.XMin, value.XMax, value.YMin, value.YMax));
        }

        public void SetLimits(double xMin, double xMax, double yMin, double yMax) =>
            Limits = new AxisLimits(xMin, xMax, yMin, yMax);

        public void AutoScale(double margin = 0.05) => Check(Native.qp_autoscale(Handle, margin));

        /// <summary>畫布像素 → 資料座標。用的是上一次 Render 的版面。</summary>
        public (double X, double Y) PixelToCoordinate(double px, double py)
        {
            Check(Native.qp_pixel_to_coord(Handle, px, py, out var x, out var y));
            return (x, y);
        }

        public (double X, double Y) CoordinateToPixel(double x, double y)
        {
            Check(Native.qp_coord_to_pixel(Handle, x, y, out var px, out var py));
            return (px, py);
        }

        /// <summary>拖曳平移：滑鼠移動了 (dx, dy) 像素，內容跟著走。</summary>
        public void Pan(double dxPx, double dyPx) => Check(Native.qp_pan(Handle, dxPx, dyPx));

        /// <summary>以像素 (px, py) 為定點縮放。factor &lt; 1 放大。</summary>
        public void Zoom(double factor, double px, double py) => Check(Native.qp_zoom(Handle, factor, px, py));

        // ───────────── 圖元 ─────────────

        public int Count => Check(Native.qp_plot_count(Handle));

        /// <summary>清掉所有圖元（之前拿到的 PlotItem 都會失效）。</summary>
        public void Clear() => Check(Native.qp_plot_clear(Handle));

        /// <summary>倒回剛建立的狀態，連標題、軸名、樣式一起清掉。</summary>
        public void Reset() { Check(Native.qp_plot_reset(Handle)); _title = ""; }

        public PlotItem AddScatter(double[] xs, double[] ys, Color? color = null)
        {
            int n = SameLength(xs, ys);
            return Wrap(Native.qp_add_scatter(Handle, xs, ys, n, Argb(color)));
        }

        public PlotItem AddMarkers(double[] xs, double[] ys, Color? color = null)
        {
            int n = SameLength(xs, ys);
            return Wrap(Native.qp_add_markers(Handle, xs, ys, n, Argb(color)));
        }

        /// <summary>等間距的大量資料（百萬點等級），x = i * period。</summary>
        public PlotItem AddSignal(double[] ys, double period = 1.0, Color? color = null) =>
            Wrap(Native.qp_add_signal(Handle, NotNull(ys), ys.Length, period, Argb(color)));

        /// <summary>不等間距的大量資料。xs 必須遞增。</summary>
        public PlotItem AddSignalXY(double[] xs, double[] ys, Color? color = null)
        {
            int n = SameLength(xs, ys);
            return Wrap(Native.qp_add_signal_xy(Handle, xs, ys, n, Argb(color)));
        }

        public PlotItem AddBars(double[] values, Color? color = null) =>
            Wrap(Native.qp_add_bars(Handle, NotNull(values), values.Length, Argb(color)));

        public PlotItem AddHistogram(double[] values, int binCount = 0, Color? color = null) =>
            Wrap(Native.qp_add_histogram(Handle, NotNull(values), values.Length, binCount, Argb(color)));

        /// <summary>熱圖。values[row, col]。</summary>
        public PlotItem AddHeatmap(double[,] values, Colormap colormap = Colormap.Viridis)
        {
            if (values == null) throw new ArgumentNullException(nameof(values));
            int rows = values.GetLength(0), cols = values.GetLength(1);
            var flat = new double[rows * cols];
            Buffer.BlockCopy(values, 0, flat, 0, flat.Length * sizeof(double));  // C# 的 [,] 本來就是 row-major
            return Wrap(Native.qp_add_heatmap(Handle, flat, rows, cols, (int)colormap));
        }

        public PlotItem AddHorizontalLine(double y, Color? color = null) => Wrap(Native.qp_add_hline(Handle, y, Argb(color)));
        public PlotItem AddVerticalLine(double x, Color? color = null) => Wrap(Native.qp_add_vline(Handle, x, Argb(color)));
        public PlotItem AddText(string text, double x, double y) => Wrap(Native.qp_add_text(Handle, Utf8(text), x, y));

        /// <summary>即時資料：固定容量的循環緩衝。用 PlotItem.Append 餵資料。</summary>
        public PlotItem AddStreamer(int capacity = 1000, Color? color = null) =>
            Wrap(Native.qp_add_streamer(Handle, capacity, Argb(color)));

        /// <summary>即時資料：只增不減，視野自動跟著資料走。</summary>
        public PlotItem AddLogger(Color? color = null) => Wrap(Native.qp_add_logger(Handle, Argb(color)));

        // ───────────── 輸出 ─────────────

        /// <summary>
        /// 畫進一塊 premultiplied BGRA 記憶體（WPF 的 Pbgra32、GDI+ 的 Format32bppPArgb 都是這個格式）。
        /// </summary>
        public void Render(IntPtr buffer, int width, int height, int stride) =>
            Check(Native.qp_render_bgra(Handle, width, height, buffer, stride));

        /// <summary>畫進 byte[]，大小至少 stride * height。給 WPF 的 WriteableBitmap 用。</summary>
        public void Render(byte[] buffer, int width, int height, int stride)
        {
            if (buffer == null) throw new ArgumentNullException(nameof(buffer));
            if (buffer.Length < (long)stride * height) throw new ArgumentException("buffer 太小", nameof(buffer));
            var pin = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            try { Render(pin.AddrOfPinnedObject(), width, height, stride); }
            finally { pin.Free(); }
        }

        /// <summary>畫進一張既有的 32bpp PArgb Bitmap（不會另外配置記憶體）。</summary>
        public void Render(Bitmap bitmap)
        {
            if (bitmap == null) throw new ArgumentNullException(nameof(bitmap));
            if (bitmap.PixelFormat != PixelFormat.Format32bppPArgb)
                throw new ArgumentException("Bitmap 必須是 Format32bppPArgb", nameof(bitmap));
            var rect = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
            var data = bitmap.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppPArgb);
            try { Render(data.Scan0, data.Width, data.Height, data.Stride); }
            finally { bitmap.UnlockBits(data); }
        }

        public Bitmap RenderToBitmap(int width, int height)
        {
            var bmp = new Bitmap(width, height, PixelFormat.Format32bppPArgb);
            try { Render(bmp); }
            catch { bmp.Dispose(); throw; }
            return bmp;
        }

        public void SavePng(string path, int width = 800, int height = 600, double scale = 1.0) =>
            Check(Native.qp_save_png(Handle, Utf8(path), width, height, scale));
        public void SaveSvg(string path, int width = 800, int height = 600) =>
            Check(Native.qp_save_svg(Handle, Utf8(path), width, height));
        public void SavePdf(string path, int width = 800, int height = 600) =>
            Check(Native.qp_save_pdf(Handle, Utf8(path), width, height));

        /// <summary>即時資料（Streamer／Logger）的版本號；每進一批資料就變，和上次不同才需要重畫。</summary>
        public ulong DataVersion => Native.qp_data_version(Handle);

        /// <summary>上一次 Render 花了幾毫秒。</summary>
        public double LastRenderMs => Native.qp_last_render_ms(Handle);

        // ───────────── 內部 ─────────────

        private PlotItem Wrap(IntPtr item)
        {
            if (item == IntPtr.Zero) throw new QtPlotException(-2, LastError());
            return new PlotItem(this, item);
        }

        internal static int Check(int code)
        {
            if (code < 0) throw new QtPlotException(code, LastError());
            return code;
        }

        internal static string LastError() => Utf8Ptr(Native.qp_last_error());

        internal static byte[]? Utf8(string? s) => s == null ? null : Encoding.UTF8.GetBytes(s + "\0");

        internal static uint Argb(Color? c) => c.HasValue ? (uint)c.Value.ToArgb() : 0u;

        private static string Utf8Ptr(IntPtr p)
        {
            if (p == IntPtr.Zero) return "";
            int len = 0;
            while (Marshal.ReadByte(p, len) != 0) ++len;
            var bytes = new byte[len];
            Marshal.Copy(p, bytes, 0, len);
            return Encoding.UTF8.GetString(bytes);
        }

        private static double[] NotNull(double[] a) => a ?? throw new ArgumentNullException();

        private static int SameLength(double[] xs, double[] ys)
        {
            if (xs == null) throw new ArgumentNullException(nameof(xs));
            if (ys == null) throw new ArgumentNullException(nameof(ys));
            if (xs.Length != ys.Length) throw new ArgumentException("xs 與 ys 長度不同");
            return xs.Length;
        }
    }

    /// <summary>
    /// 圖上的一個圖元。Plot.Clear()／Reset()／Remove() 之後就失效，再用會丟 QtPlotException。
    /// </summary>
    public sealed class PlotItem
    {
        private readonly Plot _plot;
        internal IntPtr Handle { get; }

        internal PlotItem(Plot plot, IntPtr handle) { _plot = plot; Handle = handle; }

        private string _legend = "";
        public string Legend
        {
            get => _legend;
            set { Plot.Check(Native.qp_item_set_legend(_plot.Handle, Handle, Plot.Utf8(value))); _legend = value ?? ""; }
        }

        private bool _visible = true;
        public bool IsVisible
        {
            get => _visible;
            set { Plot.Check(Native.qp_item_set_visible(_plot.Handle, Handle, value ? 1 : 0)); _visible = value; }
        }

        /// <summary>線色與線寬。color 傳 null 代表只改線寬。</summary>
        public PlotItem SetLine(Color? color, double width)
        {
            Plot.Check(Native.qp_item_set_line(_plot.Handle, Handle, Plot.Argb(color), width));
            return this;
        }

        public PlotItem SetMarker(MarkerShape shape, double size = 6, Color? color = null)
        {
            Plot.Check(Native.qp_item_set_marker(_plot.Handle, Handle, (int)shape, size, Plot.Argb(color)));
            return this;
        }

        /// <summary>換資料。Signal 的 xs 傳 null。</summary>
        public void SetData(double[]? xs, double[] ys)
        {
            if (ys == null) throw new ArgumentNullException(nameof(ys));
            if (xs != null && xs.Length != ys.Length) throw new ArgumentException("xs 與 ys 長度不同");
            Plot.Check(Native.qp_item_set_data(_plot.Handle, Handle, xs, ys, ys.Length));
        }

        /// <summary>熱圖的色階範圍。min &gt;= max 恢復自動。</summary>
        public void SetColorRange(double min, double max) =>
            Plot.Check(Native.qp_item_set_color_range(_plot.Handle, Handle, min, max));

        /// <summary>
        /// 餵資料給 Streamer／Logger。可以從背景執行緒呼叫（擷取卡、相機的執行緒）；
        /// UI 那邊用 PlotControl.AutoRefreshInterval 自動重畫。
        /// </summary>
        public void Append(params double[] ys) => Append(null, ys);

        public void Append(double[]? xs, double[] ys)
        {
            if (ys == null) throw new ArgumentNullException(nameof(ys));
            if (xs != null && xs.Length != ys.Length) throw new ArgumentException("xs 與 ys 長度不同");
            Plot.Check(Native.qp_item_append(_plot.Handle, Handle, xs, ys, ys.Length));
        }

        public void Remove() => Plot.Check(Native.qp_item_remove(_plot.Handle, Handle));
    }
}

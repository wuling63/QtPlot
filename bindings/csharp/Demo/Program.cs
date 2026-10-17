// QtPlot 的 C# 範例。
//
//   QtPlotDemo.CSharp.exe              開視窗
//   QtPlotDemo.CSharp.exe --smoke dir  不開視窗，把 C# → DLL 整條路跑一遍（ctest 用），通過回傳 0
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using QtPlot;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        if (args.Length >= 1 && args[0] == "--smoke")
            return Smoke(args.Length >= 2 ? args[1] : Path.GetTempPath());

        ApplicationConfiguration.Initialize();
        Application.Run(new DemoForm());
        return 0;
    }

    // ───────────── 不開視窗的檢查 ─────────────

    private static int Smoke(string dir)
    {
        int failures = 0;
        void Check(bool ok, string what)
        {
            Console.WriteLine($"  {(ok ? "ok  " : "FAIL")}  {what}");
            if (!ok) failures++;
        }

        Console.WriteLine($"QtPlot.Net → qtplot_c {Plot.Version}");
        Check(Plot.AvailableLanguages.Length >= 9, $"語言：{string.Join(",", Plot.AvailableLanguages)}");
        Check(Plot.SetLanguage("de") && !Plot.SetLanguage("zh_TW"), "Plot.SetLanguage 切換語言");
        using var plot = new Plot();

        var xs = Enumerable.Range(0, 200).Select(i => i * 0.05).ToArray();
        var ys = xs.Select(Math.Sin).ToArray();
        var s = plot.AddScatter(xs, ys, Color.Crimson);
        s.Legend = "sin(x) 中文圖例";
        s.SetLine(null, 2).SetMarker(MarkerShape.OpenCircle, 5);
        plot.Title = "C# 測試";
        plot.SetAxisLabel(AxisEdge.Bottom, "時間 (s)");
        plot.ShowLegend();
        Check(plot.Count == 1, "加入 scatter");

        // 錯誤要變成例外，而且訊息要從 DLL 帶回來
        var bars = plot.AddBars(new[] { 1.0, 2, 3 });
        try { bars.SetMarker(MarkerShape.FilledCircle); Check(false, "長條設標記應該丟例外"); }
        catch (QtPlotException e) { Check(e.Code == -4 && e.Message.Length > 0, $"錯誤轉成例外：{e.Message}"); }

        plot.AutoScale();
        using (var bmp = plot.RenderToBitmap(400, 300))
        {
            int nonWhite = 0;
            for (int y = 0; y < bmp.Height; y += 3)
                for (int x = 0; x < bmp.Width; x += 3)
                    if (bmp.GetPixel(x, y).ToArgb() != Color.White.ToArgb()) nonWhite++;
            Check(nonWhite > 100, "RenderToBitmap 畫出了東西");
        }

        var before = plot.Limits;
        plot.Pan(40, 0);
        Check(plot.Limits.XMin < before.XMin, "Pan 往右拖，視野左移");
        plot.Zoom(0.5, 200, 150);
        Check(Math.Abs((plot.Limits.XMax - plot.Limits.XMin) - (before.XMax - before.XMin) * 0.5) < 1e-9, "Zoom 0.5 範圍減半");

        // 百萬點的 Signal
        var big = new double[1_000_000];
        var rnd = new Random(1);
        for (int i = 1; i < big.Length; i++) big[i] = big[i - 1] + rnd.NextDouble() - 0.5;
        plot.Clear();
        try { s.Legend = "x"; Check(false, "Clear 後舊的 PlotItem 應該失效"); }
        catch (QtPlotException e) { Check(e.Code == -3, "Clear 後舊的 PlotItem 丟 BAD_ITEM"); }
        plot.AddSignal(big);
        plot.AutoScale();
        // 第一張包含字型快取之類的暖機，取後面幾張的最小值才是穩定的成本
        double best = double.MaxValue, bestTotal = double.MaxValue;
        using (var bmp = new Bitmap(1200, 600, System.Drawing.Imaging.PixelFormat.Format32bppPArgb))
        {
            for (int k = 0; k < 5; k++)
            {
                var sw = Stopwatch.StartNew();
                plot.Render(bmp);
                sw.Stop();
                Console.WriteLine($"        第 {k + 1} 張：{plot.LastRenderMs:F1} ms");
                if (k > 0)
                {
                    best = Math.Min(best, plot.LastRenderMs);
                    bestTotal = Math.Min(bestTotal, sw.Elapsed.TotalMilliseconds);
                }
            }
        }
        Check(true, $"百萬點 Signal：render {best:F1} ms（含 C# 端呼叫 {bestTotal:F1} ms）");

        // 背景執行緒餵資料，同時在這條執行緒畫
        plot.Clear();
        var streamer = plot.AddStreamer(5000);
        var v0 = plot.DataVersion;
        long fed = 0;
        var stop = false;
        var producer = new System.Threading.Thread(() =>
        {
            var chunk = new double[64];
            double ph = 0;
            while (!System.Threading.Volatile.Read(ref stop))
            {
                for (int i = 0; i < chunk.Length; i++) chunk[i] = Math.Sin(ph += 0.01);
                streamer.Append(chunk);
                System.Threading.Interlocked.Add(ref fed, chunk.Length);
            }
        });
        producer.Start();
        int frames = 0;
        var swThread = Stopwatch.StartNew();
        using (var bmp = new Bitmap(400, 300, System.Drawing.Imaging.PixelFormat.Format32bppPArgb))
            while (swThread.ElapsedMilliseconds < 300) { plot.Render(bmp); frames++; }
        System.Threading.Volatile.Write(ref stop, true);
        producer.Join();
        Check(frames > 3 && fed > 0 && plot.DataVersion != v0, $"背景執行緒 Append、同時 Render：{frames} 張、{fed} 點，沒有當掉");

        var png = Path.Combine(dir, "csharp_smoke.png");
        plot.SavePng(png, 800, 400);
        Check(File.Exists(png) && new FileInfo(png).Length > 1000, "SavePng");
        File.Delete(png);

        Console.WriteLine(failures == 0 ? "\n全部通過" : $"\n{failures} 項失敗");
        return failures;
    }
}

// ───────────── 視窗範例 ─────────────

internal sealed class DemoForm : Form
{
    private readonly PlotControl _plot = new() { Dock = DockStyle.Fill };
    private readonly ToolStripStatusLabel _status = new() { Spring = true, TextAlign = ContentAlignment.MiddleLeft };
    private readonly System.Windows.Forms.Timer _liveTimer = new() { Interval = 20 };
    private PlotItem? _streamer;
    private double _phase;

    public DemoForm()
    {
        Text = "QtPlot × C#";
        ClientSize = new Size(1000, 640);

        var bar = new ToolStrip();
        void Button(string text, Action action) =>
            bar.Items.Add(new ToolStripButton(text, null, (_, _) => { _liveTimer.Stop(); action(); _plot.Invalidate(); }));
        Button("散點", ShowScatter);
        Button("百萬點 Signal", ShowSignal);
        Button("熱圖", ShowHeatmap);
        Button("長條＋直方圖", ShowBars);
        Button("即時串流", ShowLive);
        bar.Items.Add(new ToolStripSeparator());
        bar.Items.Add(new ToolStripButton("深色／淺色", null, (_, _) => ToggleTheme()));
        bar.Items.Add(new ToolStripButton("存 PNG…", null, (_, _) => SavePng()));

        var statusStrip = new StatusStrip();
        statusStrip.Items.Add(_status);

        Controls.Add(_plot);
        Controls.Add(bar);
        Controls.Add(statusStrip);

        _plot.CoordinateHovered += (_, e) =>
            _status.Text = $"x = {e.X:G6}   y = {e.Y:G6}     render {_plot.Plot.LastRenderMs:F1} ms     左鍵拖曳平移・滾輪縮放・雙擊自動範圍";

        _liveTimer.Tick += (_, _) => FeedLive();
        ShowScatter();
    }

    private Plot P => _plot.Plot;
    private bool _dark;

    private void Fresh(string title)
    {
        P.Reset();
        if (_dark) P.SetTheme(PlotTheme.Dark);
        P.Title = title;
    }

    private void ShowScatter()
    {
        Fresh("散點與參考線");
        var xs = Enumerable.Range(0, 51).Select(i => i * 0.2).ToArray();
        P.AddScatter(xs, xs.Select(Math.Sin).ToArray()).Legend = "sin";
        P.AddScatter(xs, xs.Select(Math.Cos).ToArray()).SetMarker(MarkerShape.OpenSquare, 6).Legend = "cos";
        P.AddHorizontalLine(0.5, Color.Gray).SetLine(null, 1);
        P.AddText("峰值", Math.PI / 2, 1.05);
        P.SetAxisLabel(AxisEdge.Bottom, "角度 (rad)");
        P.SetAxisLabel(AxisEdge.Left, "數值");
        P.ShowLegend(Corner.BottomLeft);
        P.AutoScale();
    }

    private void ShowSignal()
    {
        Fresh("1,000,000 點的 Signal（每像素抽樣，縮放平移都即時）");
        var ys = new double[1_000_000];
        var rnd = new Random(0);
        for (int i = 1; i < ys.Length; i++) ys[i] = ys[i - 1] + rnd.NextDouble() - 0.5;
        P.AddSignal(ys, 1.0 / 1000).Legend = "隨機漫步";
        P.SetAxisLabel(AxisEdge.Bottom, "時間 (s)");
        P.ShowLegend();
        P.AutoScale();
    }

    private void ShowHeatmap()
    {
        Fresh("熱圖");
        const int n = 120;
        var v = new double[n, n];
        for (int r = 0; r < n; r++)
            for (int c = 0; c < n; c++)
                v[r, c] = Math.Sin(r * 0.1) * Math.Cos(c * 0.07) + 0.3 * Math.Sin((r + c) * 0.05);
        P.AddHeatmap(v, QtPlot.Colormap.Turbo);
        P.AutoScale(0);
    }

    private void ShowBars()
    {
        Fresh("長條圖與直方圖");
        P.AddBars(new[] { 3.0, 5, 2, 7, 4 }).Legend = "月營收";
        var rnd = new Random(2);
        var samples = Enumerable.Range(0, 2000).Select(_ => 10 + 2 * Gaussian(rnd)).ToArray();
        P.AddHistogram(samples, 30, Color.FromArgb(120, 214, 39, 40)).Legend = "樣本分布";
        P.ShowLegend();
        P.AutoScale();
    }

    private void ShowLive()
    {
        Fresh("即時串流（每 20 ms 餵 5 點）");
        _streamer = P.AddStreamer(1000);
        _streamer.Legend = "感測器";
        P.SetLimits(0, 1000, -1.5, 1.5);
        P.ShowLegend();
        _liveTimer.Start();
    }

    private void FeedLive()
    {
        if (_streamer == null) return;
        var chunk = new double[5];
        for (int i = 0; i < chunk.Length; i++)
        {
            _phase += 0.03;
            chunk[i] = Math.Sin(_phase) + 0.2 * Math.Sin(_phase * 7.3);
        }
        _streamer.Append(chunk);
        _plot.Invalidate();
    }

    private void ToggleTheme()
    {
        _dark = !_dark;
        P.SetTheme(_dark ? PlotTheme.Dark : PlotTheme.Light);
        _plot.Invalidate();
    }

    private void SavePng()
    {
        using var dlg = new SaveFileDialog { Filter = "PNG|*.png|SVG|*.svg|PDF|*.pdf", FileName = "plot.png" };
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        var size = _plot.ClientSize;
        switch (Path.GetExtension(dlg.FileName).ToLowerInvariant())
        {
            case ".svg": P.SaveSvg(dlg.FileName, size.Width, size.Height); break;
            case ".pdf": P.SavePdf(dlg.FileName, size.Width, size.Height); break;
            default: P.SavePng(dlg.FileName, size.Width, size.Height, 2.0); break;
        }
    }

    private static double Gaussian(Random r) =>
        Math.Sqrt(-2 * Math.Log(1 - r.NextDouble())) * Math.Cos(2 * Math.PI * r.NextDouble());

    protected override void Dispose(bool disposing)
    {
        if (disposing) _liveTimer.Dispose();
        base.Dispose(disposing);
    }
}

// PlotControl.cs — WinForms 控制項：把 Plot 畫在畫面上，並支援滑鼠平移與縮放。
//
//   左鍵拖曳   平移
//   滾輪       以游標為中心縮放
//   雙擊       自動範圍
//
// 改完圖的內容（加資料、換樣式）之後呼叫 Refresh()（或 Invalidate()）重畫。
// 資料從背景執行緒餵進來時設 AutoRefreshInterval（毫秒）：有新資料才重畫。
using System;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;

namespace QtPlot
{
    public class PlotControl : Control
    {
        private Bitmap? _buffer;
        private Point _lastMouse;
        private bool _dragging;

        [Browsable(false), DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public Plot Plot { get; } = new Plot();

        private readonly Timer _autoRefresh = new();
        private ulong _lastVersion;

        /// <summary>
        /// 每隔幾毫秒看一次即時資料有沒有新進來，有才重畫。0 代表關掉。
        /// 背景執行緒用 PlotItem.Append 餵資料時用這個，不必（也不能）從背景執行緒叫 Invalidate。
        /// </summary>
        [DefaultValue(0)]
        public int AutoRefreshInterval
        {
            get => _autoRefresh.Enabled ? _autoRefresh.Interval : 0;
            set
            {
                if (value <= 0) { _autoRefresh.Stop(); return; }
                _autoRefresh.Interval = value;
                _autoRefresh.Start();
            }
        }

        /// <summary>滾輪每一格縮放的倍率。</summary>
        [DefaultValue(1.15)]
        public double WheelZoomFactor { get; set; } = 1.15;

        /// <summary>座標換算後的游標位置改變時觸發（游標在繪圖區外時也會觸發）。</summary>
        public event EventHandler<PlotMouseEventArgs>? CoordinateHovered;

        public PlotControl()
        {
            // 整張圖都由我們自己畫，不要 WinForms 先擦背景（會閃）
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
                     ControlStyles.Selectable, true);
            _autoRefresh.Tick += (_, _) =>
            {
                var v = Plot.DataVersion;
                if (v == _lastVersion) return;  // 沒有新資料：不重畫
                _lastVersion = v;
                Invalidate();
            };
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            int w = Math.Max(1, ClientSize.Width), h = Math.Max(1, ClientSize.Height);
            if (_buffer == null || _buffer.Width != w || _buffer.Height != h)
            {
                _buffer?.Dispose();
                _buffer = new Bitmap(w, h, PixelFormat.Format32bppPArgb);
            }
            Plot.Render(_buffer);
            e.Graphics.DrawImageUnscaled(_buffer, 0, 0);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);
            Focus();
            if (e.Button == MouseButtons.Left)
            {
                _dragging = true;
                _lastMouse = e.Location;
                Capture = true;
            }
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e);
            if (_dragging)
            {
                Plot.Pan(e.X - _lastMouse.X, e.Y - _lastMouse.Y);
                _lastMouse = e.Location;
                Invalidate();
            }
            if (CoordinateHovered != null && _buffer != null)
            {
                var (x, y) = Plot.PixelToCoordinate(e.X, e.Y);
                CoordinateHovered(this, new PlotMouseEventArgs(e.X, e.Y, x, y));
            }
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            base.OnMouseUp(e);
            if (e.Button == MouseButtons.Left)
            {
                _dragging = false;
                Capture = false;
            }
        }

        protected override void OnMouseWheel(MouseEventArgs e)
        {
            base.OnMouseWheel(e);
            if (_buffer == null) return;  // 還沒畫過，不知道版面
            // 往前滾放大（視野變小），所以 factor < 1
            double factor = Math.Pow(1.0 / WheelZoomFactor, e.Delta / 120.0);
            Plot.Zoom(factor, e.X, e.Y);
            Invalidate();
        }

        protected override void OnMouseDoubleClick(MouseEventArgs e)
        {
            base.OnMouseDoubleClick(e);
            if (e.Button != MouseButtons.Left) return;
            Plot.AutoScale();
            Invalidate();
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _autoRefresh.Dispose();
                _buffer?.Dispose();
                Plot.Dispose();
            }
            base.Dispose(disposing);
        }
    }

    public sealed class PlotMouseEventArgs : EventArgs
    {
        public int PixelX { get; }
        public int PixelY { get; }
        public double X { get; }
        public double Y { get; }
        public PlotMouseEventArgs(int px, int py, double x, double y) { PixelX = px; PixelY = py; X = x; Y = y; }
    }
}

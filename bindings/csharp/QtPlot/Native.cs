// Native.cs — qtplot_c.dll 的 P/Invoke 宣告，一對一對應 src/qtplot_c/qtplot_c.h。
//
// 字串用 byte[]（UTF-8，結尾補 0）傳，不用 UnmanagedType.LPUTF8Str：
// 那個列舉值舊版 .NET Framework 沒有，用 byte[] 在哪個執行環境都一樣。
using System;
using System.Runtime.InteropServices;

namespace QtPlot
{
    internal static class Native
    {
        private const string Dll = "qtplot_c";

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_version();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_last_error();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_language(byte[]? code);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_available_languages();

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_plot_create();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern void qp_plot_destroy(IntPtr plot);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_plot_clear(IntPtr plot);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_plot_reset(IntPtr plot);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_plot_count(IntPtr plot);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_title(IntPtr plot, byte[]? text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_axis_label(IntPtr plot, int axis, byte[]? text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_style(IntPtr plot, int style);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_legend(IntPtr plot, int visible, int corner);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_grid(IntPtr plot, int visible);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_antialias(IntPtr plot, int enabled);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_font(IntPtr plot, byte[]? family, double scale);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_legend_layout(IntPtr plot, int placement, int horizontal, int columns);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_legend_border(IntPtr plot, uint argb, double width, int pattern);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_legend_background(IntPtr plot, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_legend_symbols(IntPtr plot, int filled, uint outlineArgb, double outlineWidth);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_legend_font(IntPtr plot, byte[]? family, double size, int bold, int italic, uint argb);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_axis_color(IntPtr plot, int axis, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_axis_ticks(IntPtr plot, int axis, int direction, double majorLength, double minorLength, int mirror);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_axis_tick_marker(IntPtr plot, int axis, int shape, double size, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_axis_log(IntPtr plot, int axis, int enabled, int labelFormat, int labelMinor);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_set_limits(IntPtr plot, double xMin, double xMax, double yMin, double yMax);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_get_limits(IntPtr plot, out double xMin, out double xMax, out double yMin, out double yMax);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_autoscale(IntPtr plot, double margin);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_pixel_to_coord(IntPtr plot, double px, double py, out double x, out double y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_coord_to_pixel(IntPtr plot, double x, double y, out double px, out double py);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_pan(IntPtr plot, double dxPx, double dyPx);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_zoom(IntPtr plot, double factor, double px, double py);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_scatter(IntPtr plot, double[] xs, double[] ys, int count, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_markers(IntPtr plot, double[] xs, double[] ys, int count, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_signal(IntPtr plot, double[] ys, int count, double period, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_signal_xy(IntPtr plot, double[] xs, double[] ys, int count, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_bars(IntPtr plot, double[] values, int count, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_histogram(IntPtr plot, double[] values, int count, int binCount, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_heatmap(IntPtr plot, double[] values, int rows, int cols, int colormap);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_hline(IntPtr plot, double y, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_vline(IntPtr plot, double x, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_text(IntPtr plot, byte[]? text, double x, double y);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_streamer(IntPtr plot, int capacity, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_logger(IntPtr plot, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr qp_add_live_spectrum(IntPtr plot, int fftSize, double sampleRate, uint argb);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_item_remove(IntPtr plot, IntPtr item);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_item_set_visible(IntPtr plot, IntPtr item, int visible);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_item_set_legend(IntPtr plot, IntPtr item, byte[]? text);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_item_set_line(IntPtr plot, IntPtr item, uint argb, double width);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_item_set_marker(IntPtr plot, IntPtr item, int shape, double size, uint argb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_item_set_data(IntPtr plot, IntPtr item, double[]? xs, double[] ys, int count);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_item_set_color_range(IntPtr plot, IntPtr item, double min, double max);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_item_append(IntPtr plot, IntPtr item, double[]? xs, double[] ys, int count);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_spectrum_set_analysis(IntPtr plot, IntPtr item, int window, double averaging, int peakHold, double peakHoldDecayDb);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_spectrum_set_display(IntPtr plot, IntPtr item, double minDb, double maxDb, int peakLabels, uint fillArgb);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_welch(double[] samples, int count, double sampleRate, int segmentSize, double overlap, int window, double[]? outFreq, double[]? outDb, int capacity);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_find_peaks(double[] freqs, double[] db, int count, int maxPeaks, double minProminenceDb, double minSpacingHz, double minFrequency, double[] outFreq, double[] outDb);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_render_bgra(IntPtr plot, int width, int height, IntPtr buffer, int stride);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_save_png(IntPtr plot, byte[]? path, int width, int height, double scale);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_save_svg(IntPtr plot, byte[]? path, int width, int height);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int qp_save_pdf(IntPtr plot, byte[]? path, int width, int height);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern double qp_last_render_ms(IntPtr plot);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern ulong qp_data_version(IntPtr plot);
    }
}

using System;
using System.Runtime.InteropServices;

public class WallpaperPlugin
{
	[DllImport("WallpaperPlugin")]
	public static extern int wallpaper_plugin_init();

	[DllImport("WallpaperPlugin")]
	public static extern void wallpaper_plugin_quit();

	[DllImport("WallpaperPlugin")]
	public static extern void wallpaper_plugin_upate(int count, Wallpaper[] wallpapers);

	[DllImport("WallpaperPlugin")]
	public static extern int wallpaper_plugin_number_of_monitors();

	[DllImport("WallpaperPlugin")]
	public static extern RECT wallpaper_plugin_get_monitor_rect(int monitor_index);

	[StructLayout(LayoutKind.Sequential)]
	public struct RECT
	{
		public int Left;
		public int Top;
		public int Right;
		public int Bottom;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct Wallpaper
	{
		public int X;
		public int Y;
		public IntPtr Texture;

		public Wallpaper(int x, int y, IntPtr texture)
		{
			this.X = x;
			this.Y = y;
			this.Texture = texture;
		}
	}
}

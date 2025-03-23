using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

public class Wallpaper : MonoBehaviour
{

	// Start is called before the first frame update
	List<WallpaperPlugin.RECT> rect = new();
	List<WallpaperPlugin.Wallpaper> wallpapers = new();
	public Camera[] cameras;
	void Start()
	{
		int a = WallpaperPlugin.wallpaper_plugin_init();

		int count = WallpaperPlugin.wallpaper_plugin_number_of_monitors();
		for (int i = 0; i < count && i < cameras.Length; i++) {
			WallpaperPlugin.RECT r = WallpaperPlugin.wallpaper_plugin_get_monitor_rect(i);
			rect.Add(r);

			RenderTexture texture = new RenderTexture(r.Right - r.Left, r.Bottom - r.Top, 1, RenderTextureFormat.ARGB32);
			cameras[i].targetTexture = texture;
			wallpapers.Add(new WallpaperPlugin.Wallpaper(r.Left, 0, texture.GetNativeTexturePtr()));
		}
	}

	private void OnDestroy()
	{
		WallpaperPlugin.wallpaper_plugin_quit();
	}

	// Update is called once per frame
	void Update()
	{
		WallpaperPlugin.wallpaper_plugin_upate(wallpapers.Count, wallpapers.ToArray());
	}
}

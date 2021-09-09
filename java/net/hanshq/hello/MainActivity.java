package net.hanshq.hello;

import android.app.*;
import android.os.*;
import android.widget.*;
import android.provider.*;
import android.content.*;
import android.content.res.*;
import android.net.*;
import java.io.*;
import java.util.concurrent.*;

public class MainActivity extends Activity {
	static {
		System.loadLibrary("somelib");
	}

	public native String getMessage();
	public native int apkTest(String filename);
	public native int setAssetManager(AssetManager am);
	public native void disposeAssetManager();

	private void wtf(String s) {
		Toast.makeText(this, s, Toast.LENGTH_LONG).show();
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		try {
			super.onCreate(savedInstanceState);
			if (false) {
				new AlertDialog.Builder(this).setMessage("噔噔咚！库加载不进来！")
					.setPositiveButton("哼哼", new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int id) {
							// Handle Ok
						}
					})
				.create().show();
				return;
			}
			setAssetManager(getAssets());
			openFileOutput("a.apk", Build.VERSION.SDK_INT >= 24 ? MODE_PRIVATE : MODE_WORLD_READABLE | MODE_WORLD_WRITEABLE).close();

			wtf(apkTest(new File(getFilesDir(), "a.apk").getAbsolutePath())+".");

			Intent intent = new Intent(Intent.ACTION_VIEW);
			Uri uri;
			// Although content:// URIs have stayed around since API level 1, earlier versions of Android are unable to read APKs under content://, logging “Skipping dir: /files/a.apk”.
			if (Build.VERSION.SDK_INT >= 24) {
				// getApplicationContext().getPackageName().toString() + ".fileProvider"
				//wtf("我已经锁定了你的 IP 地址："+java.net.InetAddress.getLocalHost().getHostAddress()+"（迫真）");
				uri = FileProvider.getUriForFile(this, "net.hanshq.hello.FileProvider", new File(getFilesDir(), "a.apk"));
				//wtf("WTF. " + uri.toString());
			} else {
				wtf("Good. " + getFilesDir().getPath());
				uri = Uri.fromFile(new File(getFilesDir(), "a.apk"));
			}
			intent.setDataAndType(uri, "application/vnd.android.package-archive");
			intent.setFlags(1); // Intent.FLAG_GRANT_READ_URI_PERMISSION
			startActivity(intent);
		} catch (Exception e) {
			wtf("WTF: " + e + "; " + e.getStackTrace()[0]);
		}
	}

	private void shit() {
		final ContentValues values = new ContentValues();
		values.put(MediaStore.MediaColumns.DISPLAY_NAME, "屑文件");
		values.put(MediaStore.MediaColumns.MIME_TYPE, "text/plain");

		final ContentResolver resolver = getContentResolver();
		final Uri contentUri = MediaStore.Images.Media.EXTERNAL_CONTENT_URI;
		Uri uri = resolver.insert(contentUri, values);
		if (uri == null) wtf("Failed to create new MediaStore record.");

		try {
			final OutputStream stream = resolver.openOutputStream(uri, "w");
			if (stream == null) wtf("Failed to open output stream.");
			stream.write(0x41);
			stream.write(0x43);
			stream.write(0x0a);
			stream.close();
		} catch (IOException e) {
			wtf("ioexcept");
		}

	}
}

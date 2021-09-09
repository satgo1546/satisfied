package net.hanshq.hello;

import android.content.*;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.*;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;
import java.io.*;
import java.util.List;

public class FileProvider extends ContentProvider {
	private String mAuthority;
	private Context mContext;

	static String[][] getModules(Context context) {
		try {
			if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
				return new String[][]{
					{context.getFilesDir().getCanonicalPath(), "files"},
					{context.getCacheDir().getCanonicalPath(), "cache"},
					{context.getExternalFilesDir(null).getCanonicalPath(), "external-files"},
					{context.getExternalCacheDir().getCanonicalPath(), "external-cache"},
					{"", "root"}
				};
			} else {
				return new String[][]{
					{context.getFilesDir().getCanonicalPath(), "files"},
					{context.getCacheDir().getCanonicalPath(), "cache"},
					{"", "root"}
				};
			}
		} catch (IOException e) {
			throw new RuntimeException("shit");
		}
	}

	static Uri getUriForFile(Context context, String authority, File file) {
		try {
			String path = file.getCanonicalPath();
			String root = "";
			String[][] a = getModules(context);
			for (String[] x : a) {
				if (path.startsWith(x[0])) {
					root = x[1];
					path = path.substring(x[0].length() + (x[0].endsWith("/") ? 0 : 1));
					break;
				}
			}
			return new Uri.Builder().scheme("content").authority(authority).appendPath(root).appendEncodedPath(Uri.encode(path)).build();
		} catch (IOException e) {
			throw new RuntimeException("shit");
		}
	}

	static File getFileForUri(Context context, String authority, Uri uri) {
		try {
			String root = "";
			List<String> path = uri.getPathSegments();
			String a[][] = getModules(context);
			for (String[] x : a) {
				if (path.get(0).equals(x[1])) {
					root = x[0] + "/";
					break;
				}
			}
			return new File(root, path.get(1)).getCanonicalFile();
		} catch (IOException e) {
			throw new RuntimeException("shit");
		}
	}

	static String mime(String filename) {
		int lastDot = filename.lastIndexOf(".");
		if (lastDot >= 0) {
			String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(filename.substring(lastDot + 1));
			if (type != null) return type;
		}
		return "application/octet-stream";
	}

	@Override
	public boolean onCreate() {
		return true;
	}

	@Override
	public void attachInfo(Context context, ProviderInfo info) {
		super.attachInfo(context, info);
		assert (!info.exported);
		assert (info.grantUriPermissions);
		mAuthority = info.authority;
		mContext = context;
	}

	@Override
	public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
		// ContentProvider has already checked granted permissions
		if (projection == null) projection = new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
		File file = getFileForUri(mContext, mAuthority, uri);
		String[] cols = new String[projection.length];
		Object[] values = new Object[projection.length];
		for (int i = 0; i < projection.length; i++) {
			cols[i] = projection[i];
			switch (projection[i]) {
			case (OpenableColumns.DISPLAY_NAME):
				values[i] = file.getName();
				break;
			case (OpenableColumns.SIZE):
				values[i] = file.length();
				break;
			}
		}
		MatrixCursor cursor = new MatrixCursor(cols, 1);
		cursor.addRow(values);
		return cursor;
	}

	@Override
	public String getType(Uri uri) {
		// The returned string should start with “vnd.android.cursor.{item or dir}/”
		return mime(getFileForUri(mContext, mAuthority, uri).getName());
	}

	@Override
	public Uri insert(Uri uri, ContentValues values) {
		throw new UnsupportedOperationException("No external inserts");
	}

	@Override
	public int delete(Uri uri, String selection, String[] selectionArgs) {
		throw new UnsupportedOperationException("No external updates");
	}

	@Override
	public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
		return getFileForUri(mContext, mAuthority, uri).delete() ? 1 : 0;
	}

	@Override
	public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
		int modeBits;
		switch (mode) {
		case "r":
			modeBits = ParcelFileDescriptor.MODE_READ_ONLY;
			break;
		case "w":
		case "wt":
			modeBits = (ParcelFileDescriptor.MODE_WRITE_ONLY | ParcelFileDescriptor.MODE_CREATE | ParcelFileDescriptor.MODE_TRUNCATE);
			break;
		case "wa":
			modeBits = (ParcelFileDescriptor.MODE_WRITE_ONLY | ParcelFileDescriptor.MODE_CREATE | ParcelFileDescriptor.MODE_APPEND);
			break;
		case "rw":
			modeBits = (ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_CREATE);
			break;
		case "rwt":
			modeBits = (ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_CREATE | ParcelFileDescriptor.MODE_TRUNCATE);
			break;
		default:
			throw new IllegalArgumentException("Invalid mode: $mode");
		}
		return ParcelFileDescriptor.open(getFileForUri(mContext, mAuthority, uri), modeBits);
	}
}

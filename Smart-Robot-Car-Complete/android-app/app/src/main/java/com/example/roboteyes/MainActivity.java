package com.example.roboteyes;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.util.Size;

import androidx.camera.core.CameraSelector;
import androidx.camera.core.ImageAnalysis;
import androidx.camera.core.ImageProxy;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.google.common.util.concurrent.ListenableFuture;

import fi.iki.elonen.NanoHTTPD;
import fi.iki.elonen.NanoHTTPD.IHTTPSession;
import fi.iki.elonen.NanoHTTPD.Response;
import fi.iki.elonen.NanoHTTPD.Response.Status;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.Inet4Address;
import java.net.NetworkInterface;
import java.nio.ByteBuffer;
import java.util.Collections;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private static final int CAMERA_PERMISSION = 101;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private ExecutorService cameraExecutor;
    private volatile byte[] latestJpeg;
    private CameraStreamServer streamServer;
    private TextView addressText;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        FrameLayout root = new FrameLayout(this);
        root.addView(new RobotEyesView(this),
                new FrameLayout.LayoutParams(-1, -1));

        addressText = new TextView(this);
        addressText.setTextColor(0xFFFFFFFF);
        addressText.setTextSize(12);
        addressText.setPadding(12, 8, 12, 8);
        addressText.setBackgroundColor(0x88000000);
        addressText.setText("Starting camera server...");
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(-2, -2);
        lp.gravity = android.view.Gravity.BOTTOM | android.view.Gravity.CENTER_HORIZONTAL;
        root.addView(addressText, lp);
        setContentView(root);

        cameraExecutor = Executors.newSingleThreadExecutor();
        startServer();
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
                == PackageManager.PERMISSION_GRANTED) {
            startFrontCamera();
        } else {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.CAMERA}, CAMERA_PERMISSION);
        }
        hideSystemUi();
    }

    private void startServer() {
        try {
            streamServer = new CameraStreamServer();
            streamServer.start(NanoHTTPD.SOCKET_READ_TIMEOUT, false);
            String ip = getLocalIpAddress();
            addressText.setText("Laptop dashboard: http://" + ip + ":8080");
        } catch (IOException e) {
            addressText.setText("Camera server failed: " + e.getMessage());
        }
    }

    private void startFrontCamera() {
        ListenableFuture<ProcessCameraProvider> future =
                ProcessCameraProvider.getInstance(this);
        future.addListener(() -> {
            try {
                ProcessCameraProvider provider = future.get();
                ImageAnalysis analysis = new ImageAnalysis.Builder()
                        .setTargetResolution(new Size(640, 480))
                        .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                        .build();
                analysis.setAnalyzer(cameraExecutor, this::analyzeFrame);
                provider.unbindAll();
                provider.bindToLifecycle(this, CameraSelector.DEFAULT_FRONT_CAMERA, analysis);
            } catch (Exception e) {
                runOnUiThread(() -> addressText.setText("Front camera error: " + e.getMessage()));
            }
        }, ContextCompat.getMainExecutor(this));
    }

    private void analyzeFrame(ImageProxy image) {
        try {
            latestJpeg = imageToJpeg(image);
        } catch (Exception ignored) {
        } finally {
            image.close();
        }
    }

    private byte[] imageToJpeg(ImageProxy image) {
        ImageProxy.PlaneProxy[] planes = image.getPlanes();
        int width = image.getWidth(), height = image.getHeight();
        ByteBuffer y = planes[0].getBuffer();
        ByteBuffer u = planes[1].getBuffer();
        ByteBuffer v = planes[2].getBuffer();

        int ySize = y.remaining();
        int uvSize = u.remaining() + v.remaining();
        byte[] nv21 = new byte[width * height + (width * height / 2)];

        // Copy Y plane, respecting row stride.
        int yRowStride = planes[0].getRowStride();
        int yPixelStride = planes[0].getPixelStride();
        int pos = 0;
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                nv21[pos++] = y.get(row * yRowStride + col * yPixelStride);
            }
        }

        // Interleave V and U samples for NV21.
        int uvRowStride = planes[1].getRowStride();
        int uvPixelStride = planes[1].getPixelStride();
        for (int row = 0; row < height / 2; row++) {
            for (int col = 0; col < width / 2; col++) {
                int uvIndex = row * uvRowStride + col * uvPixelStride;
                nv21[pos++] = v.get(uvIndex);
                nv21[pos++] = u.get(uvIndex);
            }
        }

        YuvImage yuv = new YuvImage(nv21, ImageFormat.NV21, width, height, null);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        yuv.compressToJpeg(new Rect(0, 0, width, height), 65, out);
        return out.toByteArray();
    }

    private String getLocalIpAddress() {
        try {
            for (NetworkInterface ni : Collections.list(NetworkInterface.getNetworkInterfaces())) {
                for (java.net.InetAddress address : Collections.list(ni.getInetAddresses())) {
                    if (!address.isLoopbackAddress() && address instanceof Inet4Address)
                        return address.getHostAddress();
                }
            }
        } catch (Exception ignored) {}
        return "PHONE-IP";
    }

    private void hideSystemUi() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == CAMERA_PERMISSION && results.length > 0
                && results[0] == PackageManager.PERMISSION_GRANTED) startFrontCamera();
        else addressText.setText("Camera permission is required for laptop video.");
    }

    @Override protected void onDestroy() {
        super.onDestroy();
        if (streamServer != null) streamServer.stop();
        if (cameraExecutor != null) cameraExecutor.shutdown();
    }

    private class CameraStreamServer extends NanoHTTPD {
        CameraStreamServer() { super(8080); }

        @Override public Response serve(IHTTPSession session) {
            String uri = session.getUri();
            if ("/snapshot.jpg".equals(uri)) {
                byte[] frame = latestJpeg;
                if (frame == null)
                    return newFixedLengthResponse(Status.SERVICE_UNAVAILABLE,
                            "text/plain", "Waiting for camera frame");
                return newFixedLengthResponse(Status.OK, "image/jpeg",
                        new java.io.ByteArrayInputStream(frame), frame.length);
            }
            String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                    + "<title>Robot Camera Dashboard</title><style>body{margin:0;background:#10131a;color:white;font-family:Arial;text-align:center}"
                    + "header{padding:14px;background:#202938}img{width:min(94vw,960px);margin:16px auto;border-radius:12px}"
                    + ".tag{color:#9ee7ff}</style></head><body><header><h2>🤖 Smart Robot Car</h2>"
                    + "<div class='tag'>Front Camera Live Feed • Phone Camera Server</div></header>"
                    + "<img id='cam' src='/snapshot.jpg'><script>setInterval(()=>{document.getElementById('cam').src='/snapshot.jpg?t='+Date.now()},350);</script>"
                    + "<p>Keep this phone and laptop connected to the same Wi-Fi network.</p></body></html>";
            return newFixedLengthResponse(Status.OK, "text/html; charset=utf-8", html);
        }
    }
}

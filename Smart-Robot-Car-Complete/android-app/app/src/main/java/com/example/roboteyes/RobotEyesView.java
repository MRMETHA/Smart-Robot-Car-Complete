package com.example.roboteyes;

import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.ColorDrawable;
import android.view.MotionEvent;
import android.view.View;
import android.os.SystemClock;

public class RobotEyesView extends View {
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint glow = new Paint(Paint.ANTI_ALIAS_FLAG);
    private long start = SystemClock.uptimeMillis();
    private boolean auto = true;
    private float manual = 0f;
    private float downX;

    public RobotEyesView(Context c) {
        super(c);
        setBackground(new ColorDrawable(Color.BLACK));
        p.setDither(true);
        glow.setDither(true);
        setFocusable(true);
    }

    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        float w = getWidth(), h = getHeight();
        float t = (SystemClock.uptimeMillis() - start) / 1000f;

        // Smooth automatic horizontal scan: left -> center -> right -> center.
        float look = auto ? (float)Math.sin(t * 0.75) : manual;
        float cx1 = w * 0.30f, cx2 = w * 0.70f;
        float cy = h * 0.50f;
        float eyeW = Math.min(w * 0.30f, h * 0.62f);
        float eyeH = h * 0.30f;

        drawEye(c, cx1, cy, eyeW, eyeH, look, false, t);
        drawEye(c, cx2, cy, eyeW, eyeH, look, true, t);

        // Small hidden touch hint: tap pauses/continues automatic animation.
        if (auto) {
            p.setColor(Color.argb(20,255,255,255));
            p.setTextSize(Math.max(10, h * 0.018f));
            p.setTextAlign(Paint.Align.CENTER);
            c.drawText("AUTO", w/2f, h - h*0.035f, p);
        }
        postInvalidateOnAnimation();
    }

    private void drawEye(Canvas c, float cx, float cy, float ew, float eh, float look, boolean right, float t) {
        Path eye = new Path();
        float topY = cy - eh*0.35f;
        float bottomY = cy + eh*0.65f;
        float left = cx - ew/2f, rightX = cx + ew/2f;
        eye.moveTo(left, topY + eh*0.12f);
        eye.quadTo(cx, topY - eh*0.03f, rightX, topY + eh*0.12f);
        eye.quadTo(cx + ew*0.34f, bottomY, cx, bottomY);
        eye.quadTo(cx - ew*0.34f, bottomY, left, topY + eh*0.12f);
        eye.close();

        // Soft white glow around the eye.
        glow.setStyle(Paint.Style.FILL);
        glow.setColor(Color.WHITE);
        glow.setShadowLayer(eh*0.18f, 0, 0, Color.WHITE);
        setLayerType(View.LAYER_TYPE_SOFTWARE, glow);
        c.drawPath(eye, glow);
        glow.clearShadowLayer();

        p.setStyle(Paint.Style.FILL);
        p.setColor(Color.WHITE);
        c.drawPath(eye, p);

        // Slight grey lower gradient imitation using translucent black bands.
        p.setColor(Color.argb(28,0,0,0));
        c.drawOval(cx-ew*0.42f, cy+eh*0.20f, cx+ew*0.42f, cy+eh*0.72f, p);

        // Pupil follows the same horizontal look direction.
        float pupilX = cx + look * ew * 0.20f;
        float pupilY = cy + eh * 0.10f;
        float pr = eh * 0.27f;
        p.setColor(Color.rgb(4,4,9));
        c.drawCircle(pupilX, pupilY, pr, p);
        p.setColor(Color.WHITE);
        c.drawCircle(pupilX - pr*0.42f, pupilY - pr*0.42f, pr*0.12f, p);

        // Dark upper eyelid, angled inward for the expressive look in the reference.
        Path lid = new Path();
        float inner = right ? cx - ew*0.47f : cx + ew*0.47f;
        float outer = right ? cx + ew*0.47f : cx - ew*0.47f;
        float angle = right ? -0.12f : 0.12f;
        lid.moveTo(outer, topY - eh*0.03f);
        lid.lineTo(inner, topY + eh*(0.27f + angle));
        lid.lineTo(inner, topY + eh*0.06f);
        lid.close();
        p.setColor(Color.BLACK);
        c.drawPath(lid, p);

        // Blink every ~6 seconds for a brief closing animation.
        float phase = (t % 6.0f);
        if (phase > 5.72f && phase < 5.98f) {
            float q = (phase - 5.72f) / 0.26f;
            float close = q < 0.5f ? q*2f : (1f-q)*2f;
            p.setColor(Color.BLACK);
            c.drawRect(cx-ew*0.55f, cy-eh*0.55f, cx+ew*0.55f, cy+eh*0.12f*close, p);
        }
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        if (e.getAction() == MotionEvent.ACTION_DOWN) { downX = e.getX(); return true; }
        if (e.getAction() == MotionEvent.ACTION_UP) {
            float dx = e.getX() - downX;
            if (Math.abs(dx) < getWidth()*0.08f) {
                auto = !auto;
            } else {
                auto = false;
                manual = Math.max(-1f, Math.min(1f, dx / (getWidth()*0.35f)));
            }
            invalidate();
            return true;
        }
        return true;
    }
}

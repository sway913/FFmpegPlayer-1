package com.ffmpeg.media;

import android.graphics.Rect;

public class TimedTextEx {

    private Rect mTextBounds = null;
    private String mTextChars = null;

    public TimedTextEx(Rect bounds, String text) {
        mTextBounds = bounds;
        mTextChars = text;
    }

    public Rect getBounds() {
        return mTextBounds;
    }

    public String getText() {
        return mTextChars;
    }
}

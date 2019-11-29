package com.ffmpeg.player.activity;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import com.ffmpeg.media.MediaMetadataRetrieverEx;
import com.ffmpeg.media.MediaPlayerEx;
import com.ffmpeg.player.R;
import com.ffmpeg.media.MetadataEx;
import com.ffmpeg.media.IMediaPlayer;
import com.ffmpeg.core.utils.StringUtils;

public class AVMediaPlayerActivity extends AppCompatActivity implements View.OnClickListener,
        SurfaceHolder.Callback, SeekBar.OnSeekBarChangeListener {

    private static final String TAG = "AVMediaPlayerActivity";
    public static final String PATH = "path";

    private String mPath;
    private boolean visible = false;
    private LinearLayout mLayoutOperation;
    private Button mBtnPause;
    private TextView mTvCurrentPosition;
    private TextView mTvDuration;
    private SeekBar mSeekBar;
    private ImageView mImageCover;
    private TextView mTextMetadata;

    private SurfaceView mSurfaceView;

    private MediaPlayerEx mMediaPlayerEx;
    private MediaMetadataRetrieverEx mMediaMetadataRetrieverEx;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_media_player);
        mPath = getIntent().getStringExtra(PATH);
        initView();
        initPlayer();
        initMediaMetadataRetriever();
    }

    private void initView() {
        mLayoutOperation = (LinearLayout) findViewById(R.id.layout_operation);
        mBtnPause = (Button) findViewById(R.id.btn_pause_play);
        mBtnPause.setOnClickListener(this);

        mTvCurrentPosition = findViewById(R.id.tv_current_position);
        mTvDuration = findViewById(R.id.tv_duration);
        mSeekBar = findViewById(R.id.seekbar);
        mSeekBar.setOnSeekBarChangeListener(this);

        mImageCover = findViewById(R.id.iv_cover);
        mTextMetadata = findViewById(R.id.tv_metadata);

        mSurfaceView =  findViewById(R.id.surfaceView);
        mSurfaceView.getHolder().addCallback(this);
        mSurfaceView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction()==(MotionEvent.ACTION_DOWN)) {
                    if (visible) {
                        mLayoutOperation.setVisibility(View.VISIBLE);
                    } else {
                        mLayoutOperation.setVisibility(View.GONE);
                    }
                    visible = !visible;
                }
                return true;
            }
        });
    }

    private void initPlayer() {
        if (TextUtils.isEmpty(mPath)) {
            return;
        }
        mMediaPlayerEx = new MediaPlayerEx();
        try {
            mMediaPlayerEx.setDataSource(mPath);
        } catch (Exception e) {
            Log.e(TAG, "initPlayer: ---->");
        }
        mMediaPlayerEx.setOnPreparedListener(new IMediaPlayer.OnPreparedListener() {
            @Override
            public void onPrepared(IMediaPlayer mp) {
                Log.d(TAG, "onPrepared: ");
                mMediaPlayerEx.start();
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        int viewWidth  = mSurfaceView.getWidth();
                        int viewHeight;
                        if (mMediaPlayerEx.getRotate() % 180 != 0) {
                            viewHeight = viewWidth * mMediaPlayerEx.getVideoWidth() / mMediaPlayerEx.getVideoHeight();
                        } else {
                            viewHeight = viewWidth * mMediaPlayerEx.getVideoHeight() / mMediaPlayerEx.getVideoWidth();
                        }
                        ViewGroup.LayoutParams layoutParams = mSurfaceView.getLayoutParams();
                        layoutParams.width = viewWidth;
                        layoutParams.height = viewHeight;
                        mSurfaceView.setLayoutParams(layoutParams);

                        mTvCurrentPosition.setText(StringUtils.generateStandardTime(Math.max(mMediaPlayerEx.getCurrentPosition(), 0)));
                        mTvDuration.setText(StringUtils.generateStandardTime(Math.max(mMediaPlayerEx.getDuration(), 0)));
                        mSeekBar.setMax((int) Math.max(mMediaPlayerEx.getDuration(), 0));
                        mSeekBar.setProgress((int)Math.max(mMediaPlayerEx.getCurrentPosition(), 0));
                    }
                });
            }
        });
        mMediaPlayerEx.setOnErrorListener(new IMediaPlayer.OnErrorListener() {
            @Override
            public boolean onError(IMediaPlayer mp, int what, int extra) {
                Log.d(TAG, "onError: what = " + what + ", msg = " + extra);
                return false;
            }
        });
        mMediaPlayerEx.setOnCompletionListener(new IMediaPlayer.OnCompletionListener() {
            @Override
            public void onCompletion(IMediaPlayer mp) {

            }
        });
        mMediaPlayerEx.setOnCurrentPositionListener(new MediaPlayerEx.OnCurrentPositionListener() {
            @Override
            public void onCurrentPosition(final long current, final long duration) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mTvCurrentPosition.setText(StringUtils.generateStandardTime(current));
                        mTvDuration.setText(StringUtils.generateStandardTime(duration));
                    }
                });
            }
        });
        try {
            mMediaPlayerEx.setOption(MediaPlayerEx.OPT_CATEGORY_PLAYER, "vcodec", "h264_mediacodec");
            mMediaPlayerEx.prepareAsync();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void initMediaMetadataRetriever() {
        if (TextUtils.isEmpty(mPath)) {
            return;
        }
        new Thread(new Runnable() {
            @Override
            public void run() {
                // 异步截屏回调
                mMediaMetadataRetrieverEx = new MediaMetadataRetrieverEx();
                mMediaMetadataRetrieverEx.setDataSource(mPath);
                byte[] data = mMediaMetadataRetrieverEx.getEmbeddedPicture();
                if (data != null) {
                    final Bitmap bitmap = BitmapFactory.decodeByteArray(data, 0, data.length);
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            mImageCover.setImageBitmap(bitmap);
                        }
                    });
                }

                final MetadataEx metadata = mMediaMetadataRetrieverEx.getMetadata();
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (metadata != null) {
                            mTextMetadata.setText(metadata.toString());
                        }
                    }
                });
            }
        }).start();

    }

    @Override
    protected void onPause() {
        super.onPause();
        if (mMediaPlayerEx != null) {
            mMediaPlayerEx.pause();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mMediaPlayerEx != null) {
            mMediaPlayerEx.resume();
        }
    }

    @Override
    protected void onDestroy() {
        if (mMediaPlayerEx != null) {
            mMediaPlayerEx.stop();
            mMediaPlayerEx.release();
            mMediaPlayerEx = null;
        }
        if (mMediaMetadataRetrieverEx != null) {
            mMediaMetadataRetrieverEx.release();
            mMediaMetadataRetrieverEx = null;
        }
        super.onDestroy();
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();
        if (id == R.id.btn_pause_play) {
            if (mMediaPlayerEx.isPlaying()) {
                mMediaPlayerEx.pause();
                mBtnPause.setBackgroundResource(R.drawable.ic_player_play);
            } else {
                mMediaPlayerEx.resume();
                mBtnPause.setBackgroundResource(R.drawable.ic_player_pause);
            }
        }
    }

    private int mProgress;
    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            mProgress = progress;
            Log.d(TAG, "onProgressChanged: progress = " + progress);
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        if (mMediaPlayerEx != null) {
            mMediaPlayerEx.seekTo(mProgress);
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (mMediaPlayerEx != null) {
            mMediaPlayerEx.setDisplay(holder);
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }



}

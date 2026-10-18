#include "spotifyDisplay.h"

#include "touchScreen.h"

#include <TFT_eSPI.h>
// A library for checking if the reset button has been pressed twice
// Can be used to enable config mode
// Can be installed from the library manager (Search for "ESP_DoubleResetDetector")
// https://github.com/khoih-prog/ESP_DoubleResetDetector

#include <JPEGDEC.h>
// Library for decoding Jpegs from the API responses
//
// Can be installed from the library manager (Search for "JPEGDEC")
// https://github.com/bitbank2/JPEGDEC

// -------------------------------
// Putting this stuff outside the class because
// I can't easily pass member functions in as callbacks for jpegdec

// -------------------------------

TFT_eSPI tft = TFT_eSPI();
JPEGDEC jpeg;
TFT_eSprite textSprite = TFT_eSprite(&tft); // used only while text is scrolling

const char *ALBUM_ART = "/album.jpg";

// This next function will be called during decoding of the jpeg file to
// render each block to the Matrix.  If you use a different display
// you will need to adapt this function to suit.
int JPEGDraw(JPEGDRAW *pDraw) {
  // Stop further decoding as image is running off bottom of screen
  if (pDraw->y >= tft.height()) return 0;
  tft.pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  return 1;
}

fs::File myfile;

void *myOpen(const char *filename, int32_t *size) {
  myfile = SPIFFS.open(filename);
  *size = myfile.size();
  return &myfile;
}

void myClose(void *handle) {
  if (myfile) myfile.close();
}

int32_t myRead(JPEGFILE *handle, uint8_t *buffer, int32_t length) {
  if (!myfile) return 0;
  return myfile.read(buffer, length);
}

int32_t mySeek(JPEGFILE *handle, int32_t position) {
  if (!myfile) return 0;
  return myfile.seek(position);
}

class CheapYellowDisplay : public SpotifyDisplay {
  public:
    void displaySetup(SpotifyArduino *spotifyObj) {
      spotify_display = spotifyObj;

      touchSetup(spotifyObj);

      Serial.println("cyd display setup");
      setWidth(320);
      setHeight(240);

      setImageHeight(150);
      setImageWidth(150);

      // Start the tft display and set it to black
      tft.init();
      tft.setRotation(1);
      tft.fillScreen(TFT_BLACK);
    }

    void showDefaultScreen() {
      tft.fillScreen(TFT_BLACK);
      drawTouchButtons(false, false, false, false);
    }

    void displayTrackProgress(long progress, long duration) {
      //  Serial.print("Elapsed time of song (ms): ");
      //  Serial.print(progress);
      //  Serial.print(" of ");
      //  Serial.println(duration);
      //  Serial.println();

      float percentage = ((float)progress / (float)duration) * 100;
      int clampedPercentage = (int)percentage;
      // Serial.println(clampedPercentage);
      int barXWidth = map(clampedPercentage, 0, 100, 0, screenWidth - 40);
      // Serial.println(barXWidth);

      int progressStartY = 150 + 5;

      // Draw outer Rectangle, in theory we only need to do this once!
      tft.drawRect(19, progressStartY, screenWidth - 38, 20, TFT_WHITE);

      // Draw the white portion of the filled bar
      tft.fillRect(20, progressStartY + 1, barXWidth, 18, TFT_WHITE);

      // Fill whats left black
      tft.fillRect(20 + barXWidth, progressStartY + 1, (screenWidth - 20) - (20 + barXWidth), 18, TFT_BLACK);
    }

    void printCurrentlyPlayingToScreen(CurrentlyPlaying currentlyPlaying) {
      stopTextScroll(); // new song, drop any scrolling in progress

      // Clear the text
      int maxTextWidth = textViewWidth();
      tft.fillRect(0, textStartY, screenWidth, screenHeight - textStartY, TFT_BLACK);

      // Keep the full text around in case the user taps to scroll
      setLine(0, currentlyPlaying.trackName, maxTextWidth);
      setLine(1, currentlyPlaying.artists[0].artistName, maxTextWidth);
      setLine(2, currentlyPlaying.albumName, maxTextWidth);

      for (int i = 0; i < NUM_LINES; i++) {
        drawFittedLine(i);
      }
    }

    void checkForInput() {
      updateTextScroll(); // advance the scroll animation (no-op if not scrolling)

      if (millis() > touchScreenCoolDownTime && handleTouched()) {
        if (textAreaTouched) {
          // Tap below the progress bar scrolls any long text once; never a button press
          handleTextTap();
          touchScreenCoolDownTime = millis() + textTapCoolDownInterval;
          return;
        }

        drawTouchButtons(previousTrackStatus, nextTrackStatus, pausePlayToggle, isCurrentlyPlaying);

        if (previousTrackStatus) {
          spotify_display->previousTrack();
        } else if (nextTrackStatus) {
          spotify_display->nextTrack();
        } else if (pausePlayToggle) {
          if (isCurrentlyPlaying) {
            spotify_display->pause();
          } else {
            spotify_display->play();
          }
        }

        drawTouchButtons(false, false, false, false);

        if (pausePlayToggle) {
          displayImage();
        }

        requestDueTime = 0;                                               // Some button has been pressed and acted on, it surely impacts the status so force a refresh
        touchScreenCoolDownTime = millis() + touchScreenCoolDownInterval; // Cool the touch off
      }
    }

    // Image Related
    void clearImage() {
      int imagePosition = screenCenterX - (imageWidth / 2);
      tft.fillRect(imagePosition, 0, imageWidth + 2, imageHeight, TFT_BLACK);
    }

    boolean processImageInfo(CurrentlyPlaying currentlyPlaying) {
      SpotifyImage currentlyPlayingMedImage = currentlyPlaying.albumImages[currentlyPlaying.numImages - 2];

      if (!albumDisplayed || !isSameAlbum(currentlyPlayingMedImage.url)) {
        // We have a differenent album than we currently have displayed
        albumDisplayed = false;
        setImageHeight(currentlyPlayingMedImage.height / 2); // medium image is 300, we are going to scale it to half
        setImageWidth(currentlyPlayingMedImage.width / 2);
        setAlbumArtUrl(currentlyPlayingMedImage.url);
        return true;
      }

      return false;
    }

    int displayImage() {
      int imageStatus = displayImageUsingFile(_albumArtUrl);

      Serial.print("imageStatus: ");
      Serial.println(imageStatus);

      if (imageStatus == 1) {
        albumDisplayed = true;
        return imageStatus;
      }

      return imageStatus;
    }

    void drawWifiManagerMessage(WiFiManager *myWiFiManager) {
      Serial.println("Entered Conf Mode");
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawCentreString("Entered Conf Mode:", screenCenterX, 5, 2);
      tft.drawString("Connect to the following WIFI AP:", 5, 28, 2);
      tft.setTextColor(TFT_BLUE, TFT_BLACK);
      tft.drawString(myWiFiManager->getConfigPortalSSID(), 20, 48, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Password:", 5, 64, 2);
      tft.setTextColor(TFT_BLUE, TFT_BLACK);
      tft.drawString("thing123", 20, 82, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);

      tft.drawString("If it doesn't AutoConnect, use this IP:", 5, 110, 2);
      tft.setTextColor(TFT_BLUE, TFT_BLACK);
      tft.drawString(WiFi.softAPIP().toString(), 20, 128, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    void drawRefreshTokenMessage() {
      Serial.println("Refresh Token Mode");
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawCentreString("Refresh Token Mode:", screenCenterX, 5, 2);
      tft.drawString("You need to authorize this device to use", 5, 28, 2);
      tft.drawString("your spotify account.", 5, 46, 2);

      tft.drawString("Visit the following address and follow", 5, 82, 2);
      tft.drawString("the instrucitons:", 5, 100, 2);
      tft.setTextColor(TFT_BLUE, TFT_BLACK);
      tft.drawString(WiFi.localIP().toString(), 10, 128, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

  private:
    unsigned long touchScreenCoolDownInterval = 200; // How long after a touch press do we accept another (0.2 seconds). There is also an APi request inbetween
    unsigned long touchScreenCoolDownTime;           // time when cool down has expired

    // ---- Text scrolling (title, artist, album) ----
    // A tap below the progress bar scrolls every line that is too long: out to the end,
    // pause, back to the start, then the truncated version is restored.
    enum ScrollPhase { SCROLL_IDLE, SCROLL_START_PAUSE, SCROLL_OUT, SCROLL_END_PAUSE, SCROLL_BACK };

    struct TextLine {
      String full;                       // untruncated text
      String fitted;                     // truncated text shown when not scrolling
      int width = 0;                     // pixel width of full
      bool overflows = false;            // true if full doesn't fit on screen
      int y = 0;                         // top of the line on screen
      int offset = 0;                    // px the text is shifted left
      ScrollPhase phase = SCROLL_IDLE;
      unsigned long nextTime = 0;        // when this line next needs attention
    };

    static const int NUM_LINES = 3;      // title, artist, album
    TextLine lines[NUM_LINES];
    int textStartY = 150 + 30;           // top of the first line
    int lineSpacing = 18;
    bool scrolling = false;

    const int scrollStep = 2;                        // px per frame
    const unsigned long scrollFrameInterval = 30;    // ms between frames
    const unsigned long scrollEndPause = 1000;       // ms to pause at each end
    unsigned long textTapCoolDownInterval = 500;     // ms to ignore taps after a text tap

    int textViewWidth() {
      return screenWidth - 20; // 10px margin each side
    }

    void setLine(int i, const String &text, int maxWidth) {
      lines[i].full = text;
      lines[i].width = tft.textWidth(text, 2);
      lines[i].overflows = lines[i].width > maxWidth;
      lines[i].fitted = fitText(text, maxWidth, 2);
      lines[i].y = textStartY + i * lineSpacing;
      lines[i].offset = 0;
      lines[i].phase = SCROLL_IDLE;
    }

    void drawFittedLine(int i) {
      tft.drawCentreString(lines[i].fitted, screenCenterX, lines[i].y, 2);
    }

    void restoreLine(int i) {
      tft.fillRect(0, lines[i].y, screenWidth, lineSpacing, TFT_BLACK);
      drawFittedLine(i);
    }

    void drawLineFrame(int i) {
      textSprite.fillSprite(TFT_BLACK);
      textSprite.drawString(lines[i].full, -lines[i].offset, 0, 2);
      textSprite.pushSprite(10, lines[i].y);
    }

    void handleTextTap() {
      if (scrolling) return; // one pass at a time

      bool anyOverflow = false;
      for (int i = 0; i < NUM_LINES; i++) {
        if (lines[i].overflows) anyOverflow = true;
      }
      if (!anyOverflow) return; // everything fits, nothing to scroll

      startTextScroll();
    }

    void startTextScroll() {
      // One sprite is shared by all lines and only exists while scrolling,
      // so the RAM is free for HTTPS/JPEG the rest of the time
      if (textSprite.createSprite(textViewWidth(), tft.fontHeight(2)) == nullptr) {
        Serial.println("Failed to allocate text sprite");
        return;
      }
      textSprite.setTextColor(TFT_WHITE, TFT_BLACK);

      scrolling = true;
      for (int i = 0; i < NUM_LINES; i++) {
        if (!lines[i].overflows) continue;

        lines[i].offset = 0;
        lines[i].phase = SCROLL_START_PAUSE;
        lines[i].nextTime = millis() + scrollEndPause; // linger on the start of the text
        drawLineFrame(i);
      }
    }

    // Abandon scrolling without redrawing (caller redraws the text area itself)
    void stopTextScroll() {
      if (!scrolling) return;

      scrolling = false;
      textSprite.deleteSprite();
      for (int i = 0; i < NUM_LINES; i++) {
        lines[i].phase = SCROLL_IDLE;
      }
    }

    void updateTextScroll() {
      if (!scrolling) return;

      bool anyActive = false;
      unsigned long now = millis();

      for (int i = 0; i < NUM_LINES; i++) {
        TextLine &l = lines[i];
        if (l.phase == SCROLL_IDLE) continue;

        anyActive = true;
        if (now < l.nextTime) continue;

        int maxOffset = l.width - textViewWidth();

        switch (l.phase) {
          case SCROLL_START_PAUSE:
            l.phase = SCROLL_OUT;
            break;

          case SCROLL_OUT:
            l.offset += scrollStep;
            if (l.offset >= maxOffset) {
              l.offset = maxOffset;
              l.phase = SCROLL_END_PAUSE;
              l.nextTime = now + scrollEndPause;
            } else {
              l.nextTime = now + scrollFrameInterval;
            }
            drawLineFrame(i);
            break;

          case SCROLL_END_PAUSE:
            l.phase = SCROLL_BACK;
            break;

          case SCROLL_BACK:
            l.offset -= scrollStep;
            if (l.offset <= 0) {
              l.offset = 0;
              l.phase = SCROLL_IDLE;
              restoreLine(i);
            } else {
              l.nextTime = now + scrollFrameInterval;
              drawLineFrame(i);
            }
            break;

          default:
            break;
        }
      }

      if (!anyActive) {
        // Every line has finished its pass
        scrolling = false;
        textSprite.deleteSprite();
      }
    }

    // Returns text trimmed with "..." so it fits within maxWidth pixels
    String fitText(const String &text, int maxWidth, int font) {
      if (tft.textWidth(text, font) <= maxWidth) return text;

      String t = text;
      while (t.length() > 0 && tft.textWidth(t + "...", font) > maxWidth) {
        t.remove(t.length() - 1);
        // Don't leave half of a multi-byte UTF-8 character at the end
        while (t.length() > 0 && ((uint8_t)t[t.length() - 1] & 0xC0) == 0x80) t.remove(t.length() - 1);
        if (t.length() > 0 && ((uint8_t)t[t.length() - 1] & 0xC0) == 0xC0) t.remove(t.length() - 1);
      }
      t.trim(); // avoid "word ..."
      return t + "...";
    }

    int displayImageUsingFile(char *albumArtUrl) {

      // In this example I reuse the same filename
      // over and over, maybe saving the art using
      // the album URI as the name would be better
      // as you could save having to download them each
      // time, but this seems to work fine.
      if (SPIFFS.exists(ALBUM_ART) == true) {
        Serial.println("Removing existing image");
        SPIFFS.remove(ALBUM_ART);
      }

      fs::File f = SPIFFS.open(ALBUM_ART, "w+");
      
      if (!f) {
        Serial.println("file open failed");
        return -1;
      }

      bool gotImage = spotify_display->getImage(albumArtUrl, &f);

      // Make sure to close the file!
      f.close();

      if (gotImage) {
        return drawImagefromFile(ALBUM_ART);
      } else {
        return -2;
      }
    }

    int drawImagefromFile(const char *imageFileUri) {
      unsigned long lTime = millis();
      lTime = millis();
      jpeg.open((const char *)imageFileUri, myOpen, myClose, myRead, mySeek, JPEGDraw);
      jpeg.setPixelType(1);
      int imagePosition = screenCenterX - (imageWidth / 2);
      // decode will return 1 on sucess and 0 on a failure
      int decodeStatus = jpeg.decode(imagePosition, 0, JPEG_SCALE_HALF);
      // jpeg.decode(45, 0, 0);
      jpeg.close();
      Serial.print("Time taken to decode and display Image (ms): ");
      Serial.println(millis() - lTime);

      return decodeStatus;
    }

    void drawTouchButtons(bool backStatus, bool forwardStatus, bool pausePlayStatus, bool isPlayingStatus) {
      int buttonCenterY = 75;
      int leftButtonCenterX = 40;
      int rightButtonCenterX = screenWidth - leftButtonCenterX;

      // Draw back Button
      tft.fillCircle(leftButtonCenterX, buttonCenterY, 16, TFT_BLACK);
      tft.drawCircle(leftButtonCenterX, buttonCenterY, 16, TFT_WHITE);

      if (backStatus) {
        tft.fillCircle(leftButtonCenterX, buttonCenterY, 15, TFT_GREEN);
      } else {
        tft.drawCircle(leftButtonCenterX, buttonCenterY, 15, TFT_WHITE);
      }

      tft.fillTriangle(leftButtonCenterX - 4, buttonCenterY, leftButtonCenterX + 6, buttonCenterY - 10, leftButtonCenterX + 6, buttonCenterY + 10, TFT_WHITE);
      tft.drawRect(leftButtonCenterX - 6, buttonCenterY - 10, 2, 20, TFT_WHITE);

      // Draw forward Button
      tft.fillCircle(rightButtonCenterX, buttonCenterY, 16, TFT_BLACK);
      tft.drawCircle(rightButtonCenterX, buttonCenterY, 16, TFT_WHITE);

      if (forwardStatus) {
        tft.fillCircle(rightButtonCenterX, buttonCenterY, 15, TFT_GREEN);
      }

      tft.fillTriangle(rightButtonCenterX + 4, buttonCenterY, rightButtonCenterX - 6, buttonCenterY - 10, rightButtonCenterX - 6, buttonCenterY + 10, TFT_WHITE);
      tft.drawRect(rightButtonCenterX + 6, buttonCenterY - 10, 2, 20, TFT_WHITE);

      // Draw pause/play Button
      if(pausePlayStatus) {
        tft.drawCircle(screenCenterX, buttonCenterY, 16, TFT_WHITE);

        if(isPlayingStatus) {
          // Draw pause symbol
          tft.fillRect(screenCenterX - 6, buttonCenterY - 10, 3, 20, TFT_WHITE);
          tft.fillRect(screenCenterX + 3, buttonCenterY - 10, 3, 20, TFT_WHITE);
        } else {
          // Draw play symbol
          tft.fillTriangle(screenCenterX - 4, buttonCenterY - 8, screenCenterX - 4, buttonCenterY + 8, screenCenterX + 8, buttonCenterY, TFT_WHITE);
        }
      }
    }
};
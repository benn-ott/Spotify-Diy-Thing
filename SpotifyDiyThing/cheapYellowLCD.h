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
TFT_eSprite titleSprite = TFT_eSprite(&tft); // used only while the title is scrolling

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
      stopTitleScroll(false); // new song, drop any scrolling in progress

      // Clear the text
      int textStartY = titleY;
      int maxTextWidth = titleViewWidth();
      tft.fillRect(0, textStartY, screenWidth, screenHeight - textStartY, TFT_BLACK);

      // Keep the full title around in case the user taps it to scroll
      fullTitle = currentlyPlaying.trackName;
      titleTextWidth = tft.textWidth(fullTitle, 2);
      titleOverflows = titleTextWidth > maxTextWidth;
      fittedTitle = fitText(fullTitle, maxTextWidth, 2);

      tft.drawCentreString(fittedTitle, screenCenterX, textStartY, 2);
      tft.drawCentreString(fitText(currentlyPlaying.artists[0].artistName, maxTextWidth, 2), screenCenterX, textStartY + 18, 2);
      tft.drawCentreString(fitText(currentlyPlaying.albumName, maxTextWidth, 2), screenCenterX, textStartY + 36, 2);
    }

    void checkForInput() {
      updateTitleScroll(); // advance the title scroll animation (no-op if not scrolling)

      if (millis() > touchScreenCoolDownTime && handleTouched()) {
        if (textAreaTouched) {
          // Tap below the progress bar toggles title scrolling; never a button press
          handleTitleTap();
          touchScreenCoolDownTime = millis() + titleTapCoolDownInterval;
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

    // ---- Title scrolling ----
    int titleY = 150 + 30;                               // top of the title line
    String fullTitle;                                    // untruncated title
    String fittedTitle;                                  // truncated title shown when not scrolling
    int titleTextWidth = 0;                              // pixel width of fullTitle
    bool titleOverflows = false;                         // true if fullTitle doesn't fit on screen
    bool scrolling = false;
    int scrollOffset = 0;                                // px the text is shifted left
    int scrollDir = 1;                                   // 1 = scrolling to the end, -1 = back
    unsigned long nextScrollTime = 0;
    const int scrollStep = 2;                            // px per frame
    const unsigned long scrollFrameInterval = 30;        // ms between frames
    const unsigned long scrollEndPause = 1000;           // ms to pause at each end
    unsigned long titleTapCoolDownInterval = 500;        // ms to ignore taps after a title tap

    int titleViewWidth() {
      return screenWidth - 20; // 10px margin each side
    }

    void handleTitleTap() {
      if (!titleOverflows) return; // title fits, nothing to scroll

      if (scrolling) {
        stopTitleScroll(true);
      } else {
        startTitleScroll();
      }
    }

    void startTitleScroll() {
      // Sprite only exists while scrolling, so the RAM is free for HTTPS/JPEG the rest of the time
      if (titleSprite.createSprite(titleViewWidth(), tft.fontHeight(2)) == nullptr) {
        Serial.println("Failed to allocate title sprite");
        return;
      }
      titleSprite.setTextColor(TFT_WHITE, TFT_BLACK);

      scrolling = true;
      scrollOffset = 0;
      scrollDir = 1;
      drawTitleFrame();
      nextScrollTime = millis() + scrollEndPause; // linger on the start of the title
    }

    void stopTitleScroll(bool redraw) {
      if (!scrolling) return;

      scrolling = false;
      titleSprite.deleteSprite();

      if (redraw) {
        tft.fillRect(0, titleY, screenWidth, 18, TFT_BLACK);
        tft.drawCentreString(fittedTitle, screenCenterX, titleY, 2);
      }
    }

    void drawTitleFrame() {
      titleSprite.fillSprite(TFT_BLACK);
      titleSprite.drawString(fullTitle, -scrollOffset, 0, 2);
      titleSprite.pushSprite(10, titleY);
    }

    void updateTitleScroll() {
      if (!scrolling || millis() < nextScrollTime) return;

      scrollOffset += scrollDir * scrollStep;
      int maxOffset = titleTextWidth - titleViewWidth();

      if (scrollOffset >= maxOffset) {
        scrollOffset = maxOffset;
        scrollDir = -1;
        nextScrollTime = millis() + scrollEndPause;
      } else if (scrollOffset <= 0) {
        scrollOffset = 0;
        scrollDir = 1;
        nextScrollTime = millis() + scrollEndPause;
      } else {
        nextScrollTime = millis() + scrollFrameInterval;
      }

      drawTitleFrame();
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
# Spotify-Diy-Thing

This is a fork of https://github.com/witnessmenow/Spotify-Diy-Thing. Consult that project for setup instructions. 

The webflasher for this fork is availble [here](https://benn-ott.github.io/Spotify-Diy-Thing/). (Chrome & Edge only)

## Changes

### Setup

Due to [changes to Spotify's security requirements](https://developer.spotify.com/blog/2025-02-12-increasing-the-security-requirements-for-integrating-with-spotify), the old methods for authorizing the DIY Thing no longer work. There are probably ways to work around this in the code, but it's easier to just bypass the authenetication flow. We can do this through a simple Python script that you run locally to fetch a refresh token that you input during setup.
- First, install **Spotipy**: `pip install spotipy`.
- Download `get_refresh_token.py` from this repo and edit the `CLIENT_ID` and `CLIENT_SECRET` strings to match your application.
- In your Spotify developer dashboard, set the redirect URI to `http://127.0.0.1:8888/callback`.
- Run the Python script, which will launch the Spotify authorization flow. The script should grab the URL and display the token, but you may need to copy and paste the URL yourself.
- Save your refresh token, and enter it when you enter the client ID and client secret in while setting up your Wi-Fi. This should enable you to skip the authenication step from the original project.

### Code

- Removes all NFC and matrix display code.
- Removes cyd2usb option (newer versions of this board no longer have the issues that made this option necesasry).

import spotipy
from spotipy.oauth2 import SpotifyOAuth

# --------------------------------------------------------
# Fill in your Spotify app credentials here
# --------------------------------------------------------
CLIENT_ID     = "YOUR CODE HERE"
CLIENT_SECRET = "YOUR CODE HERE"
REDIRECT_URI  = "http://127.0.0.1:8888/callback"
# --------------------------------------------------------

SCOPE = "user-read-playback-state user-modify-playback-state"

auth_manager = SpotifyOAuth(
    client_id=CLIENT_ID,
    client_secret=CLIENT_SECRET,
    redirect_uri=REDIRECT_URI,
    scope=SCOPE,
    open_browser=True,
)

print("\nOpening Spotify login in your browser...")
print("After you approve access, you'll be redirected to a page that may show an error.")
print("That's fine — just copy the ENTIRE URL from your browser's address bar and paste it here.\n")

# This will open the browser and wait for you to paste the redirect URL
token_info = auth_manager.get_access_token(as_dict=True)

refresh_token = token_info.get("refresh_token")

if refresh_token:
    print("\n✅ Success! Here is your refresh token:")
    print("-" * 60)
    print(refresh_token)
    print("-" * 60)
    print("\nPaste this into the 'Refresh Token' field in the SpotifyDiy config page.")
else:
    print("\n❌ Failed to get refresh token. Check your CLIENT_ID and CLIENT_SECRET and try again.")
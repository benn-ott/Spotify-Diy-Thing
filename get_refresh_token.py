from spotipy.oauth2 import SpotifyOAuth

CLIENT_ID = "YOUR_CLIENT_ID"
CLIENT_SECRET = "YOUR_CLIENT_SECRET"
REDIRECT_URI = "http://127.0.0.1:8888/callback"  # must match dashboard EXACTLY
SCOPE = "user-read-playback-state user-modify-playback-state"

auth_manager = SpotifyOAuth(
    client_id=CLIENT_ID,
    client_secret=CLIENT_SECRET,
    redirect_uri=REDIRECT_URI,
    scope=SCOPE,
    open_browser=False,
)

auth_url = auth_manager.get_authorize_url()
print(f"1. Open this URL, log in, and click Agree:\n{auth_url}\n")
response_url = input(
    "2. Paste the FULL URL of the page you land on afterwards (even if it "
    "fails to load) here: "
).strip()

code = auth_manager.parse_response_code(response_url)
auth_manager.get_access_token(code, as_dict=False)  # caches the token(s)

token_info = auth_manager.get_cached_token()
print("\nAccess token:", token_info["access_token"])
print("Refresh token:", token_info["refresh_token"])
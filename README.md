Context

The ESP32-S3 Navidrome player has working audio streaming, display rendering (text + album art), and serial commands. The next step is making it a standalone device: touch gestures and rotary encoder for input, plus a menu system to browse and play music without a serial connection.

Hardware

- Touch: CST816D, I2C addr 0x15, SDA=GPIO11, SCL=GPIO12, IRQ=GPIO9, RST=GPIO10
- Encoder: A=GPIO8, B=GPIO7 (no push button)
- Library: fbiego/CST816S (compatible with CST816D)
- Hardware gestures: Swipe Up/Down/Left/Right, Tap, Double Click, Long Press — all detected in hardware

Gesture Mapping

┌─────────────┬──────────────┬────────────┬────────────┬─────────────┬────────────┬────────────────┐
│   Context   │   Swipe Up   │ Swipe Down │ Swipe Left │ Swipe Right │    Tap     │      Knob      │
├─────────────┼──────────────┼────────────┼────────────┼─────────────┼────────────┼────────────────┤
│ Menu        │ Back         │ Select/OK  │ —          │ —           │ Select/OK  │ Scroll up/down │
├─────────────┼──────────────┼────────────┼────────────┼─────────────┼────────────┼────────────────┤
│ Now Playing │ Back to menu │ —          │ Prev song  │ Next song   │ Play/Pause │ Volume         │
└─────────────┴──────────────┴────────────┴────────────┴─────────────┴────────────┴────────────────┘

Navigation Structure

MAIN MENU → [Favorites, Artists, Playlists, Settings]
  │
  ├─ Favorites → Starred albums (getStarred2) → Album songs (getAlbum) → Play
  ├─ Artists → Artist list (getArtists) → Albums (getArtist) → Songs (getAlbum) → Play
  ├─ Playlists → Playlist list (getPlaylists) → Songs (getPlaylist) → Play
  └─ Settings → TBD

Back gesture pops the navigation stack at every level.

File Structure

Split the current monolithic main.cpp into focused modules:

┌───────────────┬────────────────────────────────────────────────────────────────────┐
│     File      │                           Responsibility                           │
├───────────────┼────────────────────────────────────────────────────────────────────┤
│ config.h      │ Pin defs, colors, layout constants, extern credential declarations │
├───────────────┼────────────────────────────────────────────────────────────────────┤
│ api.h/cpp     │ Subsonic auth, HTTP calls, JSON parsing → structured data          │
├───────────────┼────────────────────────────────────────────────────────────────────┤
│ display.h/cpp │ All lcd* functions, menu list renderer, cover art, TJpgDec         │
├───────────────┼────────────────────────────────────────────────────────────────────┤
│ input.h/cpp   │ CST816S touch + encoder ISR → InputAction enum                     │
├───────────────┼────────────────────────────────────────────────────────────────────┤
│ player.h/cpp  │ Audio object, play queue, playback control, audio callbacks        │
├───────────────┼────────────────────────────────────────────────────────────────────┤
│ ui.h/cpp      │ Screen state machine, navigation stack, input→action dispatch      │
├───────────────┼────────────────────────────────────────────────────────────────────┤
│ main.cpp      │ Credentials, WiFi, serial commands, setup(), loop()                │
└───────────────┴────────────────────────────────────────────────────────────────────┘

Key Data Structures

// Fixed-size to avoid heap fragmentation — allocated in PSRAM for large lists
struct MenuItem { char id[24]; char name[52]; };           // 76 bytes each

struct QueueEntry { char id[24]; char title[52];           // Play queue
                    char artist[40]; char coverArt[24]; }; // 140 bytes each

enum ScreenType { SCREEN_MAIN_MENU, SCREEN_STARRED_ALBUMS, SCREEN_ARTIST_LIST,
                  SCREEN_ARTIST_ALBUMS, SCREEN_ALBUM_SONGS, SCREEN_PLAYLIST_LIST,
                  SCREEN_PLAYLIST_SONGS, SCREEN_NOW_PLAYING };

struct ScreenState { ScreenType type; char contextId[24];  // Nav stack entry
                     int cursorIndex; int scrollOffset; };

Menu Renderer Layout (360×360 round AMOLED)

- Title: centered at y=30, scale 2, white
- 7 visible items starting at y=70, 40px spacing
- Selected item: white text with highlight bar
- Unselected: gray text on black
- Scroll indicators (arrows) at top/bottom if list extends beyond view
- Left margin 40px to avoid round edge clipping

API Endpoints

┌─────────────────────┬───────────────────────┬────────────────────────────────────┐
│      Function       │       Endpoint        │        JSON path for items         │
├─────────────────────┼───────────────────────┼────────────────────────────────────┤
│ apiGetStarredAlbums │ getStarred2.view      │ starred2.album[]                   │
├─────────────────────┼───────────────────────┼────────────────────────────────────┤
│ apiGetArtists       │ getArtists.view       │ artists.index[].artist[] (flatten) │
├─────────────────────┼───────────────────────┼────────────────────────────────────┤
│ apiGetArtistAlbums  │ getArtist.view?id=X   │ artist.album[]                     │
├─────────────────────┼───────────────────────┼────────────────────────────────────┤
│ apiGetAlbumSongs    │ getAlbum.view?id=X    │ album.song[]                       │
├─────────────────────┼───────────────────────┼────────────────────────────────────┤
│ apiGetPlaylists     │ getPlaylists.view     │ playlists.playlist[]               │
├─────────────────────┼───────────────────────┼────────────────────────────────────┤
│ apiGetPlaylistSongs │ getPlaylist.view?id=X │ playlist.entry[] (NOT "song")      │
└─────────────────────┴───────────────────────┴────────────────────────────────────┘

Each returns count of items populated into a caller-provided array. Empty arrays may be omitted from JSON — always null-check.

Play Queue

When user selects a song from an album/playlist:
1. Load all songs from that album/playlist into the queue
2. Start playing from the selected index
3. audio_eof_stream auto-advances to next in queue
4. Next/Prev gestures navigate within the queue
5. When queue exhausts → wrap around (or fall back to random)

Implementation Phases

Phase 1: File split (no new features)

Move existing code into the multi-file structure. Verify compilation and that serial commands + playback + display all still work.

Phase 2: Touch & encoder input

- Add fbiego/CST816S to lib_deps
- Create input.h/cpp with CST816S init + encoder ISR
- Log input events to serial for verification
- Test: twist knob, tap, swipe — verify correct actions in serial

Phase 3: Menu list renderer

- Add lcdDrawMenuList() to display.h/cpp
- Test with a hardcoded 10-item list to tune layout constants
- Adjust margins, spacing, highlight style for the round display

Phase 4: UI state machine

- Create ui.h/cpp with navigation stack and input handling
- Wire main menu (4 hardcoded items), knob scrolls, swipe-down selects
- Test: navigate main menu with gestures

Phase 5: API browsing integration

- Implement all 6 apiGetX() functions
- Wire loadScreenData() + selectCurrentItem() for each screen type
- Test all browse paths: Favorites→Album→Songs, Artists→Albums→Songs, Playlists→Songs

Phase 6: Play queue + Now Playing

- Implement queue load/play/next/prev in player.cpp
- Wire song selection → queue load → play → now-playing screen
- Implement now-playing gestures (tap=pause, swipe=next/prev, knob=volume)
- Auto-advance on stream end
- Volume overlay on knob turn (disappears after 1.5s)

Phase 7: Polish

- Empty list handling ("No items" message)
- API error handling (show error, return to previous screen)
- Queue exhaustion fallback
- Serial commands updated to use new player module

Verification

After each phase: compile, flash, test on hardware via serial monitor. Final verification: full end-to-end flow without serial — boot → main menu → browse favorites → select album → select song → plays with art → next/prev/pause via gestures → back to menu → browse artists → play album.

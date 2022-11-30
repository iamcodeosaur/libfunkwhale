# Funkwhale API library

The document about Funkwhale API is located [here](https://docs.funkwhale.audio/swagger/).

## How to build
* Create ``token.h`` with your data (``token.template.h`` is an example of the header)
* Add ``cover.jpg`` and ``test.mp3`` into a root path of the project
* Run ``make submodule init && make submodule update`` to get submodules
* ``make`` will build both id3v2lib and funkwhale api

## Dependencies
* id3v2lib (included into the project as a submodule)
* CURL
* cJSON

## TODO list

### Implement Auth and security requests API
- [x] GET /authorize
- [x] POST /api/v1/oauth/apps
- [ ] POST /api/v1/oauth/token
- [ ] POST /api/v1/auth/registration
- [ ] POST /api/v1/auth/password/reset
- [ ] GET /api/v1/users/me
- [ ] DELETE /api/v1/users/me
- [ ] POST /api/v1/users/change-email
- [ ] GET /api/v1/rate-limit

### Implement Library and metadata requests API
- [x] GET /api/v1/artists
- [ ] GET /api/v1/artists/{id}
- [ ] GET /api/v1/artists/{id}/libraries
- [x] GET /api/v1/albums
- [ ] GET /api/v1/albums/{id}
- [ ] GET /api/v1/albums/{id}/libraries
- [x] GET /api/v1/tracks
- [ ] GET /api/v1/tracks/{id}
- [ ] GET /api/v1/tracks/{id}/libraries
- [ ] GET /api/v1/listen/{uuid}
- [ ] GET /api/v1/licenses
- [ ] GET /api/v1/licenses/{code}

### Implement Uploading and audio content API
- [x] GET /api/v1/libraries
- [ ] POST /api/v1/libraries
- [ ] GET /api/v1/libraries/{uuid}
- [ ] POST /api/v1/libraries/{uuid}
- [ ] DELETE /api/v1/libraries/{uuid}
- [ ] GET /api/v1/uploads
- [x] POST /api/v1/uploads
- [ ] GET /api/v1/uploads/{uuid}
- [ ] PATCH /api/v1/uploads/{uuid}
- [ ] DELETE /api/v1/uploads/{uuid}
- [ ] GET /api/v1/uploads/{uuid}/audio-file-metadata

### Implement Channels and subscriptions API
- [x] GET /api/v1/channels
- [x] POST /api/v1/channels
- [x] GET /api/v1/channels/metadata-choices
- [ ] GET /api/v1/channels/{uuid}
- [ ] POST /api/v1/channels/{uuid}
- [ ] DELETE /api/v1/channels/{uuid}
- [ ] POST /api/v1/channels/rss-suscribe
- [ ] GET /api/v1/channels/{uuid}/rss
- [ ] POST /api/v1/channels/{uuid}/subscribe
- [ ] POST /api/v1/channels/{uuid}/unsubscribe
- [ ] GET /api/v1/subscriptions/{uuid}
- [ ] GET /api/v1/subscriptions
- [ ] GET /api/v1/subscriptions/all

### Implement Content curation API
- [ ] GET /api/v1/favorites/tracks
- [ ] POST /api/v1/favorites/tracks
- [ ] POST /api/v1/favorites/tracks/remove
- [ ] GET /api/v1/playlists
- [ ] POST /api/v1/playlists
- [ ] GET /api/v1/playlists/{id}
- [ ] POST /api/v1/playlists/{id}
- [ ] DELETE /api/v1/playlists/{id}
- [ ] GET /api/v1/playlists/{id}/tracks
- [ ] POST /api/v1/playlists/{id}/add
- [ ] POST /api/v1/playlists/{id}/move
- [ ] POST /api/v1/playlists/{id}/remove
- [ ] DELETE /api/v1/playlists/{id}/clear
- [ ] POST /api/v1/radios/sessions
- [ ] POST /api/v1/radios/tracks

### Implement User activity API
- [ ] GET /api/v1/history/listenings
- [ ] POST /api/v1/history/listenings

### Implement Other API
- [ ] GET /api/v1/search
- [ ] GET /api/v1/instance/settings
- [ ] POST /api/v1/attachments
- [ ] GET /api/v1/attachments/{uuid}
- [ ] DELETE /api/v1/attachments/{uuid}

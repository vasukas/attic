// +build !js

package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"net"
	"net/http"
	"os"
	"sync"

	"github.com/gorilla/handlers"
	"github.com/gorilla/websocket"
	"github.com/pion/webrtc/v3"
)

func objToJson(obj interface{}) []byte {
	b, err := json.Marshal(obj)
	if err != nil {
		panic(err)
	}
	return b
}

func jsonToObj(in []byte, obj interface{}) {
	err := json.Unmarshal(in, obj)
	if err != nil {
		panic(err)
	}
}

func main() {
	codecPtr := flag.String("codec", webrtc.MimeTypeVP8, "Codec string (MIME type for browser)")
	websoHost := flag.String("webso", "127.0.0.1:8081", "Websocket host address")
	udpHost := flag.String("ip", "127.0.0.1", "UDP IP (RTP receive)")
	udpPort := flag.Int("port", 10004, "UDP port (RTP receive)")
	flag.Parse()

	fmt.Printf("arguments: [%s] udp://%s:%d -> ws://%s\n", *codecPtr, *udpHost, *udpPort, *websoHost)

	// Session collection

	type Session struct {
		peerConnection *webrtc.PeerConnection
		track          *webrtc.TrackLocalStaticRTP
	}

	var sessions = make(map[uint32]*Session)  // hashmap: uint32 -> Session
	var nextSessionId uint32 = 0  // used only for this mapping
	var sessionMutex sync.Mutex

	MutateSession := func(f func()) {
		sessionMutex.Lock()
		defer sessionMutex.Unlock()
		f()
	}
	NewSession := func() uint32 {
		var sessionId uint32
		MutateSession(func() {
			for {
				if sessions[nextSessionId] == nil {
					break
				} else {
					nextSessionId += 1
				}
			}
			sessionId = nextSessionId
			nextSessionId += 1

			sessions[sessionId] = &Session{}
		})
		return sessionId
	}
	CloseSession := func(id uint32) {
		fmt.Printf("closed session %d\n", id)
		delete(sessions, id)
	}

	// Websocket server

	go func() {
		http.HandleFunc("/ffrtc", func(w http.ResponseWriter, r *http.Request) {
			fmt.Printf("http request from: %s\n", r.RemoteAddr)

			// HTTP -> Websocket

			upgrader := websocket.Upgrader{}
			upgrader.CheckOrigin = func(r *http.Request) bool { return true }

			ws, err := upgrader.Upgrade(w, r, nil)
			if err != nil {
				panic(fmt.Sprintf("upgrade to websocket failed: %s\n", err))
			}
			defer ws.Close()

			// Create session

			sessionId := NewSession()
			session := sessions[sessionId]

			fmt.Printf("created session %d for: %s\n", sessionId, r.RemoteAddr)

			peerConnection, err := webrtc.NewPeerConnection(webrtc.Configuration{})
			if err != nil {
				panic(err)
			}

			track, err := webrtc.NewTrackLocalStaticRTP(webrtc.RTPCodecCapability{MimeType: *codecPtr}, "video", "ffrtc")
			if err != nil {
				panic(err)
			}
			rtpSender, err := peerConnection.AddTrack(track)
			if err != nil {
				panic(err)
			}

            // close session on failure
			peerConnection.OnICEConnectionStateChange(func(connectionState webrtc.ICEConnectionState) {
				fmt.Printf("Connection State has changed %s \n", connectionState.String())
				switch connectionState {
				case webrtc.ICEConnectionStateDisconnected:
					fallthrough
				case webrtc.ICEConnectionStateFailed:
					fallthrough
				case webrtc.ICEConnectionStateClosed:
					MutateSession(func() { CloseSession(sessionId) })
				}
			})

			// Read incoming RTCP packets
			// Before these packets are retuned they are processed by interceptors. For things
			// like NACK this needs to be called.
			go func() {
				rtcpBuf := make([]byte, 1500) // TODO: why 1500?
				for {
					if _, _, rtcpErr := rtpSender.Read(rtcpBuf); rtcpErr != nil {
						return
					}
				}
			}()

			MutateSession(func() {
				session.peerConnection = peerConnection
				session.track = track
			})

			// Read & decode offer
			_, message, err := ws.ReadMessage()
			if err != nil {
				panic(fmt.Sprintf("ReadMessage: %s\n", err))
			}
			offer := webrtc.SessionDescription{}
			jsonToObj(message, &offer)

			var answer webrtc.SessionDescription

			// Create channel that is blocked until ICE Gathering is complete
			gatherComplete := webrtc.GatheringCompletePromise(peerConnection)

			MutateSession(func() {
				// Set the remote SessionDescription
				if err = peerConnection.SetRemoteDescription(offer); err != nil {
					panic(err)
				}

				// Create answer
				answer, err = peerConnection.CreateAnswer(nil)
				if err != nil {
					panic(err)
				}

				// Sets the LocalDescription, and starts our UDP listeners
				if err = peerConnection.SetLocalDescription(answer); err != nil {
					panic(err)
				}
			})

			// Block until ICE Gathering is complete (no trickle ICE needed)
			<-gatherComplete

			// Send answer
			answerStr := objToJson(*peerConnection.LocalDescription())
			if err := ws.WriteMessage(websocket.TextMessage, answerStr); err != nil {
				panic(fmt.Sprintf("WriteMessage: %s\n", err))
			}
		})
		fmt.Printf("Running websocket server\n")
		http.ListenAndServe(*websoHost, handlers.LoggingHandler(os.Stdout, http.DefaultServeMux))
	}()

	// Open a UDP Listener for RTP Packets

	listener, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP(*udpHost), Port: *udpPort})
	if err != nil {
		panic(err)
	}
	defer func() {
		if err = listener.Close(); err != nil {
			panic(err)
		}
	}()

	// Read RTP packets forever and send them to WebRTC clients

	inboundRTPPacket := make([]byte, 65507) // max UDP packet size
	for {
		n, _, err := listener.ReadFrom(inboundRTPPacket)
		if err != nil {
			panic(fmt.Sprintf("error during read: %s", err))
		}

		MutateSession(func() {
			for sessionId, session := range sessions {
				if session.track == nil {
					continue
				}
				if _, err = session.track.Write(inboundRTPPacket[:n]); err != nil {
					fmt.Printf("error during read [%d]: %s", sessionId, err)
					CloseSession(sessionId)
				}
			}
		})
	}
}

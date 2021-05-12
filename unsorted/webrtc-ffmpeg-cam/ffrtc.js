// Makes element to play video from server via webrtc
//   videoElement - <video> tag object
//   webSocketUrl - server url, i.e. "ws://127.0.0.1:8081/ffrtc"
function playWebrtc(videoElement, webSocketUrl)
{
    // on each failed connect attempt timeout is increased up to max.
    // after successful connect timeout is reset back to min
    var reconnectTimeoutMin = 50
    var reconnectTimeoutMax = 1000
    
    /*
       how this works:
       - webrtc peerconnection creates local SDP (session descriptor)
           which contains codec capabilities and available transports;
       - SDP offer sent to server via websocket as JSON;
       - SDP answer received from server (via websocket as JSON);
       - both local and remote SDPs are set in peerconnection;
       - webrtc automagically connects to server and handles video.
    */
    
    var reconnectTimeout = reconnectTimeoutMin
    var reconnectingAlready = false
    // sometimes websocket closes before webrtc changes state after having both SDP set
    var hasRemoteAnswer = false
    
    var pc = null // webrtc peerconnection
    var sock = null // websocket
    
    var pc_localdescr = null
    var str_localdescr = null
    
    // checks if peerconnection is active
    function isOk() {
        // pc.connectionState not implemented in Firefox 85 and older Chromes
        return pc && (pc.iceConnectionState == 'checking' || pc.iceConnectionState == 'completed' || pc.iceConnectionState == 'connected')
    }
    
    // starts reconnect attempt
    function reconnect() {
        if (reconnectingAlready) return
        console.log('begin reconnecting')
        
        stopAll()
        reconnectTimeout = Math.min(reconnectTimeout * 2, reconnectTimeoutMax)
        window.setTimeout(start, reconnectTimeout) // calls start after timeout
        reconnectingAlready = true
    }
    
    function startWS() {
        sock = new WebSocket(webSocketUrl)
        
        sock.addEventListener('open',  function() {console.log('socket open')})
        sock.addEventListener('close', function() {console.log('socket closed')})
        sock.addEventListener('error', function() {console.log('socket error')})
        
        sock.addEventListener('open', function() {
		    if (str_localdescr) {  // in case webrtc haven't created offer yet
		        sock.send(str_localdescr)
		        str_localdescr = null
		        console.log('sent local descr (WS)')
		    }
        })
        sock.addEventListener('message', function() {
		    console.log('got remote SDP\n', JSON.parse(event.data).sdp, '\n===')
		    
            pc.setLocalDescription(pc_localdescr)
            pc.setRemoteDescription(new RTCSessionDescription(JSON.parse(event.data)))
            hasRemoteAnswer = true
        })
        
        function onError() {
            if (!hasRemoteAnswer) reconnect()
        }
        sock.addEventListener('close', onError)
        sock.addEventListener('error', onError)
    }
    
    function start() {
        reconnectingAlready = false
        hasRemoteAnswer = false
        
        startWS()
        pc = new RTCPeerConnection()
    
        pc.addEventListener('icegatheringstatechange',  function() {console.log('icegatheringstatechange:',  pc.iceGatheringState )})
        pc.addEventListener('iceconnectionstatechange', function() {console.log('iceconnectionstatechange:', pc.iceConnectionState)})
        pc.addEventListener('signalingstatechange',     function() {console.log('signalingstatechange:',     pc.signalingState    )})
        
        pc.addTransceiver('video')  
        
        pc.createOffer().then(function(d) {
            //console.log('=== local SDP\n', d.sdp, '\n===')
            
            pc_localdescr = d  // can't set local description here - sometimes causes connection errors
            str_localdescr = JSON.stringify(pc_localdescr)
            
            if (sock.readyState == 1) { // if socket already connected
                sock.send(str_localdescr)
		        str_localdescr = null
		        console.log('sent local descr (PC)')
            }
        })
        
        pc.addEventListener('track', function(evt) {
            if (evt.track.kind == 'video') {
                videoElement.srcObject = evt.streams[0]
                console.log('got video track')
            }
        })
        
        pc.addEventListener('iceconnectionstatechange', function() {
            if (!isOk() && pc.iceConnectionState != 'new') {
                reconnect()
            }
            else if (pc.iceConnectionState == 'connected') {
                reconnectTimeout = reconnectTimeoutMin // reset timeout
            }
        })
    }
    
    function stopAll() {
        videoElement.srcObject = null
        if (pc) pc.close()
        if (sock) sock.close()
        pc = null
        sock = null
    }
    
    start()
	console.log('inited')
}


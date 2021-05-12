module TermAnsi where

import Text.Printf

ansiClearScreen = "\x1b[1;1H\x1b[0J" -- Cursor Position 0,0 + Erase in Display 0

ansiShowCursor = "\x1b[?25h"
ansiHideCursor = "\x1b[?25l"

ansiSetPos :: Int -> Int -> String
ansiSetPos x y = printf "\x1b[%d;%dH" (y + 1) (x + 1) -- Cursor Position

ansiResetStyle = "\x1b[0m" -- Select Graphic Rendition - Reset

ansiRed = 1
ansiGreen = 2
ansiBlue = 4
ansiBright = 8

ansiRgb r g b bright = 
    toInt r ansiRed + toInt g ansiGreen + toInt b ansiBlue + toInt bright ansiBright
    where toInt flag value = if flag then value else 0

ansiForeColor :: Int -> String
ansiForeColor color =
    if color < 8 then printf "\x1b[0;3%dm" color
                 else printf "\x1b[1;9%dm" (color - 8)

ansiBackColor :: Int -> String
ansiBackColor color =
    if color < 8 then printf "\x1b[4%dm" color
                 else printf "\x1b[10%dm" (color - 8)

ansiRequestPos = "\x1b[6n" -- Device Status Report

-- TODO: refactor both funcs below

ansiParsePos :: String -> (Int, Int)
ansiParsePos s =
    read (0,0) False $ drop 2 s
    where
        read ret snd (';':cs) =
            if snd then (0,0)
                   else read ret True cs
        read ret snd ('R':_) =
            if snd then ret
                   else (0,0)
        read (x,y) snd (c:cs) =
            read ret snd cs
            where ret = if snd then (value x, y)
                               else (x, value y)
                  value v = v * 10 + (fromEnum c - fromEnum '0')
        read ret _ [] = ret
  
{-    
ansiRemoveSequence :: (String -> Bool) -> String -> String
ansiRemoveSequence which s =
    filt s [] False
    where
        filt (c:cs) _ False = c : filt cs [] False
        filt ('\x1b':cs) seq False = filt cs ['\x1b'] True
        filt (c:cs) seq True =
            if isEnd c then unseq (seq ++ [c]) ++ filt cs [] False
                       else filt cs (seq ++ [c]) True
        isEnd c = elem c "HJhlmR"
        unseq seq = if which seq then [] else seq
-}

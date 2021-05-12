module TermIO where

import System.IO
import System.Timeout

import TermAnsi

data Request =
      RequestNone -- immediatly returns AnswerInit
    | RequestExit -- immediatly returns AnswerExit
    | RequestChar -- returns AnswerChar
    | RequestCharTimeout Int -- microseconds; returns AnswerChar or AnswerTimeout
    | RequestLine -- returns AnswerLine

data Answer =
      AnswerInit
    | AnswerExit
    | AnswerChar Char
    | AnswerLine String
    | AnswerTimeout

termInit :: IO ()
termInit = hSetBuffering stdout (BlockBuffering Nothing)

termFinish :: IO ()
termFinish = do
    hSetBuffering stdout LineBuffering -- those are probably defaults
    hSetBuffering stdin LineBuffering
    hSetEcho stdout True
    putStr ansiShowCursor
    putStrLn ansiResetStyle

termOutput :: String -> IO ()
termOutput s = do
    hSetBuffering stdout (BlockBuffering $ Just $ length s)
    putStr s
    hFlush stdout

termAnswer :: Request -> IO Answer
termAnswer RequestNone = return AnswerInit
termAnswer RequestExit = return AnswerExit

termAnswer RequestChar = do
    c <- termGetChar
    return (AnswerChar c)

termAnswer (RequestCharTimeout waitTime) = do
    c <- timeout waitTime termGetChar
    case c of
        Just c -> return (AnswerChar c)
        Nothing -> return AnswerTimeout

termAnswer RequestLine = do
    ret <- termGetLine
    return (AnswerLine ret)

termGetLine :: IO String
termGetLine = do
    hSetBuffering stdin LineBuffering
    hSetEcho stdout True
    putStr ansiShowCursor
    getLine

termGetChar :: IO Char
termGetChar = do
    hSetBuffering stdin NoBuffering
    hSetEcho stdout False
    putStr ansiHideCursor
    getChar
-- TODO: process escape-sequences

termSize :: IO (Int, Int)
termSize = do
    putStrLn $ ansiSetPos 9999 9999 ++ ansiRequestPos
    s <- termReadAnsiResponse
    return $ ansiParsePos s

termReadAnsiResponse :: IO String
termReadAnsiResponse = do
    hSetBuffering stdin NoBuffering
    c <- hGetChar stdin
    if c == 'R' then return [c]
    else do
        next <- termReadAnsiResponse 
        return (c : next)

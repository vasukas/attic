import Text.Printf

import TermAnsi
import TermGraphics
import TermIO
import Utils

main = do
    size <- termSize
    termOutput ansiClearScreen
    termOutput "Press ESC to exit...\n"
    termInit
    --_ <- terminalEchoTest AnswerInit 7
    _ <- terminalRain size AnswerInit 0 []
    termOutput ansiClearScreen
    termFinish
    putStrLn $ printf "Terminal size: %s" (show size)

-- rain test

indexList :: Int -> [Int]
indexList n =
    f 0 n
    where f i left = if left > 0 then i : f (i + 1) (left - 1) else []

makeRain (width, height) seed =
    (drops, seed + 1)
    where
        drops = map drop $ indexList num
        drop i = ((x, y), cell char 7 0)
            where
                x = mod pos' width
                y = div pos' width
                pos' = mod (width * height + pos) (width * height)
                pos = (min at at_max) * (width - 1) + randInt32 i + randInt32 life
                at_max = period - length death
                char = if at <= at_max then '/' else death !! (period - at)
                life = div time period
                at = time - life * period
                time = seed + i * max (div period num) 1
                period = 20
        num = div (width * height) 64
        death = ['.', '.', '_', '\\', 'Y', '_'] -- reverse order

makeLightning scr seed =
    if not thisFrame && not prevFrame then scr
    else
        newCells ++ map (\ (pos, c) -> (pos, c { fore = 0, back = color })) scr
        where
            newCells = map (\ pos -> (pos, cell ' ' 0 color)) allCoords
            allCoords = allPairs (indexList maxX) (indexList maxY)
            (maxX, maxY) = foldl (\ (ax, ay) ((bx, by), _) -> (max ax bx, max ay by)) (0, 0) scr
            --
            color = if thisFrame then 15 else 7
            thisFrame = mod (randInt32 seed) each == 0
            prevFrame = mod (randInt32 $ seed - 1) each == 0
            each = 24

terminalRain size answer seed oldScreen = do
    case answer of
        AnswerChar '\ESC' -> return ()
        _ -> do
            let (newScreen', newSeed) = makeRain size seed
            let newScreen = makeLightning newScreen' seed
            termOutput $ ansiClearScreen ++ toAnsiUpdate oldScreen newScreen
            answer <- termAnswer (RequestCharTimeout $ div 1000000 12)
            terminalRain size answer newSeed newScreen

-- echo test

nextColor :: Int -> Int
nextColor c = if c == 15 then 1 else c + 1

terminalEchoTest answer color = do
    case answer of
        AnswerChar '\ESC' -> return ()
        AnswerChar ch -> do
            termOutput $ ansiForeColor color ++ show ch ++ show color
            answer <- termAnswer RequestChar
            terminalEchoTest answer $ nextColor color
        _ -> do
            answer <- termAnswer RequestChar
            terminalEchoTest answer $ nextColor color

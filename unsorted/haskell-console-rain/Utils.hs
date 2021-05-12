module Utils where

headDef :: a -> [a] -> a
headDef def list =
    case list of
        [] -> def
        (x:_) -> x

tailSafe :: [a] -> [a]
tailSafe list =
    case list of
        [] -> []
        (_:xs) -> xs

listUpdateDiff :: Eq a => [a] -> [a] -> ([a], [a], [a])
listUpdateDiff old new =
    (removed, same, added)
    where
        removed = filter isRemoved old
        same = filter isSame new
        added = filter isNew new
        isRemoved x = not $ any (x ==) new
        isSame x = any (x ==) old
        isNew x = not $ any (x ==) old
    -- TODO: optimize

-- Linear congruential generator -- Turbo Pascal constants
randInt32 :: Int -> Int
randInt32 x =
    mod (a * x + c) m
    where
        a = 134775813
        c = 1
        m = 4294967296 -- 2^32

-- Turns [0,1] & [2,3] into [(0,2), (0,3), (1,2), (1,3)]
allPairs :: [a] -> [b] -> [(a, b)]
allPairs as bs = [(a, b) | a <- as, b <- bs]

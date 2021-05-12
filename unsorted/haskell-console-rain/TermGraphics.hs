module TermGraphics where

import TermAnsi
import TermIO
import Utils

data Cell = Cell {
    char :: Char,
    fore :: Int,
    back :: Int
} deriving (Eq)

type CellPos = (Int, Int)
type CellList = [(CellPos, Cell)]

cellSpace :: Cell
cellSpace = Cell {
    char = ' ',
    fore = 7,
    back = 0
}

testCell :: Cell
testCell = cellSpace { char = '.', fore = 15, back = 1 }

cell :: Char -> Int -> Int -> Cell
cell char fore back = Cell { char = char, fore = fore, back = back }

toAnsiUpdate :: CellList -> CellList -> String
toAnsiUpdate oldList newList =
    remove ++ add
    where
        remove = foldl (++) "" $ map cellposToAnsiString removedSpaces
        removedSpaces = map (\ (pos, _) -> (pos, cellSpace)) removedCs
        add = foldl (++) "" $ map cellposToAnsiString addedCs
        (addedCs, _, removedCs) = listUpdateDiff oldList newList

cellposToAnsiString :: (CellPos, Cell) -> String
cellposToAnsiString ((x, y), c) = ansiSetPos x y ++ cellToAnsiString c

cellToAnsiString :: Cell -> String
cellToAnsiString c = ansiForeColor (fore c) ++ ansiBackColor (back c) ++ [char c]

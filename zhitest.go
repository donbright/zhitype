package main

import (
	"encoding/binary"
	"bytes"
//	"time"
	"fmt"
	"os"
	"io"
)

type shortFrac uint16 // "signed fraction"?
type Fixed uint32 // fixed point 16.16
type FWord int16 // FUnits, em space 
type uFWord uint16 // FUnits, em space
type F2Dot14 uint16 // fixed point, 2.14
type longDateTime int64

type Font_directory struct {
	Offset_table Fd_offset_table
	Table_directory Fd_table_directory
}

type Fd_offset_table struct {
	Ofascaler uint32
	NumTables uint16
	SearchRange uint16
	EntrySelector uint16
	RangeShift uint16
}

type Fd_table_directory map[uint32]Fd_table_directory_entry

type Fd_table_directory_entry struct {
	Tag uint32
	CheckSum uint32
	Offset uint32
	Length uint32
}

type Head_table struct {
	Version Fixed
	FontRevision Fixed
	CheckSumAdjustment uint32
	MagicNumber uint32
	Flags uint16
	UnitsPerEm uint16
	Created longDateTime 
	Modified longDateTime 
	XMin FWord 
	YMin FWord 
	XMax FWord 
	YMax FWord 
	MacStyle uint16
	LowestRecPPEM uint16
	FontDirectionHint int16
	IndexToLocFormat int16
	GlyphDataFormat int16
}

type loca_table_short_version struct {
	offsets []uint16
}

type loca_table_long_version struct { 
	offsets []uint32
}

type cmap_table_index struct {
	version uint16
	numberSubtables uint16
}

type cmap_subtable struct {
	platformId uint16
	platformSpecificId uint16
	offset uint32
}

func (fd Font_directory) render_txt() {
	fmt.Println("Font Directory")
	fmt.Println(" Offset Table")
	fmt.Println("  ofascaler", fd.Offset_table.Ofascaler )
	fmt.Println("  numTables", fd.Offset_table.NumTables )
	fmt.Println("  searchRange", fd.Offset_table.SearchRange )
	fmt.Println("  entrySelector", fd.Offset_table.EntrySelector )
	fmt.Println("  rangeShift", fd.Offset_table.RangeShift )
	
	fmt.Println(" Table Directory")
	fmt.Println("  table   tag    checkSum  offset   length")
	for key,value := range fd.Table_directory {
		buf := new(bytes.Buffer)
		binary.Write(buf, binary.BigEndian, key)
		fmt.Printf( "  %s %08x\n", buf.String(), value )
	}
}

func (ht Head_table) render_txt() {
	fmt.Println("Head Table")
	fmt.Println(" Version ",ht.Version)
	fmt.Printf(" FontRevision 0x%08x %g\n",ht.FontRevision,float32(ht.FontRevision)/65536.0)
	fmt.Printf(" CheckSumAdjustment %08x\n",ht.CheckSumAdjustment)
	fmt.Printf(" MagicNumber %08x\n",ht.MagicNumber)
	fmt.Printf(" Flags %0.16b\n",ht.Flags)
	fmt.Println(" UnitsPerEm",ht.UnitsPerEm)
	fmt.Println(" Created",ht.Created)
	fmt.Println(" Modified",ht.Modified)
	fmt.Println(" XMin",ht.XMin)
	fmt.Println(" YMin",ht.YMin)
	fmt.Println(" XMax",ht.XMax)
	fmt.Println(" YMax",ht.YMax)
	fmt.Printf(" MacStyle %0.16b\n",ht.MacStyle)
	fmt.Println(" LowestRecPPEM",ht.LowestRecPPEM)
	fmt.Println(" FontDirectionHint",ht.FontDirectionHint)
	fmt.Println(" IndexToLocFormat",ht.IndexToLocFormat)
	fmt.Println(" GlyphDataFormat",ht.GlyphDataFormat)
}

func read_font_directory( f io.Reader, fd *Font_directory ) error {
	var err error
	if fd==nil { return err }
	err = binary.Read( f, binary.BigEndian, &fd.Offset_table)
	for i := uint16(0); i< fd.Offset_table.NumTables; i++ {
		tmp := Fd_table_directory_entry{}
		err = binary.Read( f, binary.BigEndian, &tmp )
		fd.Table_directory[tmp.Tag] = tmp
	}
	return err
}

func main() {
	//var unicode uint32 = 65

	var fd Font_directory
	fd.Table_directory = make( Fd_table_directory )
	var ht Head_table
	f, err := os.Open("FreeSerif.ttf")
	if err != nil {
		fmt.Println("file open failed", err )
		return
	}
	err = read_font_directory( f, &fd )
	err = binary.Read( f, binary.BigEndian, &ht )
	if err != nil {
		fmt.Println("binary read failed", err )
		return
	}
	fd.render_txt()
	ht.render_txt()	
}


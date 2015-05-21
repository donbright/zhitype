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

func New_Font_directory( f io.Reader ) (fd *Font_directory, err error) {
	fd = &Font_directory{}
	err = binary.Read( f, binary.BigEndian, &fd.Offset_table)
	fd.Table_directory = make( map[uint32]Fd_table_directory_entry )
	for i := uint16(0); i< fd.Offset_table.NumTables; i++ {
		tmp := Fd_table_directory_entry{}
		err = binary.Read( f, binary.BigEndian, &tmp )
		fd.Table_directory[tmp.Tag] = tmp
	}
	return fd, err
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

func New_Head_table( f io.Reader ) (ht *Head_table, err error) {
	ht = &Head_table{}
	err = binary.Read( f, binary.BigEndian, ht )
	return ht, err
}

type Loca_table_short_version struct {
	offsets []uint16
}

type Loca_table_long_version struct { 
	offsets []uint32
}

type Cmap_table struct {
	Index Cmap_table_index
	Subtables []Cmap_subtable
}

type Cmap_table_index struct {
	Version uint16
	NumberSubtables uint16
}

type Cmap_subtable struct {
	PlatformId uint16
	PlatformSpecificId uint16
	Offset uint32
}

func New_Cmap_table( f io.Reader ) (cm *Cmap_table, err error) {
	cm = &Cmap_table{}
	err = binary.Read( f, binary.BigEndian, &cm.Index )
	for i := uint16(0); i<cm.Index.NumberSubtables; i++ {
		tmp := Cmap_subtable{}
		err = binary.Read( f, binary.BigEndian, &tmp )
		cm.Subtables = append( cm.Subtables, tmp )
	}
	return cm, err
}

type Cmap_format_0 struct {
	Format uint16
	Length uint16
	Language uint16
	GlyphIndexArray [256]uint8
}

type Cmap_format_2 struct {
	Format uint16
	Length uint16
	Language uint16
	SubHeaderKeys [256]uint16
	SubHeaders []Cmap_format_2_subHeader
	GlyphIndexArray []uint16
}

type Cmap_format_2_subHeader struct {
	FirstCode uint16
	EntryCount uint16
	IdDelta int16
	IdRangeOffset uint16
}

type Cmap_format_4 struct {
	Format uint16
	Length uint16
	Language uint16
	SegCountX2 uint16
	SearchRange uint16
	EntrySelector uint16
	RangeShift uint16
	EndCode []uint16
	ReservedPad uint16
	StartCode []uint16
	IdDelta []uint16
	IdRangeOffset []uint16
	GlyphIndexArray []uint16
}

type Cmap_format_6 struct {
	Format uint16
	Length uint16
	Language uint16
	FirstCode uint16
	EntryCount uint16
	GlyphIndexArray []uint16
}

type Cmap_format_8 struct {
	Format Fixed
	Length uint32
	Language uint32
	Is32 [65536]uint8
	NGroups uint32
	Groups []Cmap_format_8_group
}

type Cmap_format_8_group struct {
	StartCharCode uint32
	EndCharCode uint32
	StartGlyphCode uint32
}

type Cmap_format_10 struct {
	Format Fixed
	Length uint32
	Language uint32
	StartCharCode uint32
	NumChars uint32
	Glyphs []uint16
}

type Cmap_format_12 struct {
	Format Fixed
	Length uint32
	Language uint32
	NGroups uint32
}

type Cmap_format_12_group struct {
	StartCharCode uint32
	EndCharCode uint32
	StartGlyphCode uint32
}

type Cmap_format_13_group struct {
	StartCharCode uint32
	EndCharCode uint32
	GlyphCode uint32
}

type Cmap_format_14 struct {
	Format uint16
	Length uint32
	NumVarSelectorRecords uint32
}

type Cmap_14_variation_selector_record struct {
	VarSelector [3]uint8
	DefaultOVSOffset uint32
	NonDefaultUVSOffset uint32
}

type Cmap_14_default_uvs_table struct {
	NumUnicodeValueRanges uint32
}

type Cmap_14_unicode_value_range struct {
	StartUnicodeValue [3]uint8
	AdditionalCount uint8
}

type Cmap_14_nondefault_uvs_table struct {
	NumUVSMappings uint32
}

type Cmap_14_uvs_mapping struct {
	UnicodeValue [3]uint8
	GlyphID uint16
}

type Glyf_description struct {
	NumberOfContours int16
	XMin FWord
	YMin FWord
	XMax FWord
	YMax FWord
}

type Glyf_definition_simple struct {
	EndPtsOfContours []uint16
	InstructionLength uint16
	Instructions []uint8
	Flags []uint8
	XCoordinateBytes []uint8 // can be uint16 or uint8
	YCoordinateBytes []uint8 // can be uint16 or uint8
}

type Glyf_definition_compound struct {
	Flags uint16
	GlyphIndex uint16
	Argument1Bytes []uint8
	Argument2Bytes []uint8
	// Transformation // ????
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

func (cm Cmap_table) render_txt() {
	fmt.Println("Cmap")
	fmt.Println("          Code1, Code2, Offset")
	for i := uint16(0); i<cm.Index.NumberSubtables; i++ {
		fmt.Printf("Subtable %5d\n",cm.Subtables[i])
	}
}

//type Ctest struct {
//	xcoord []uint32
//}

func main() {
	//ct := Ctest{ xcoord: make([]uint32,500) }

	var unicode uint32 = 65
	fmt.Println("unicode to lookup:", unicode)

	f, err := os.Open("FreeSerif.ttf")
	if err != nil {
		fmt.Println("file open failed", err )
		return
	}
	fd, err := New_Font_directory( f )
	ht, err := New_Head_table( f )
	f.Seek( int64(fd.Table_directory[0x636d6170].Offset), 0 )
	cm, err := New_Cmap_table( f )
	
	if err != nil {
		fmt.Println("binary read failed", err )
		return
	}
	fd.render_txt()
	ht.render_txt()
	cm.render_txt()
}


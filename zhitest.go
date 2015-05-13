package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"os"
)

type font_directory_offset_subtable struct {
	ofascaler uint32
	numTables uint16
	searchRange uint16
	entrySelector uint16
	rangeShift uint16
}

type table_directory_entry struct {
	//tag uint32
	checkSum uint32
	offset uint32
	length uint32
}

func main() {
	oftb := font_directory_offset_subtable{}
	f, err := os.Open("FreeSerif.ttf")
	if err != nil {
		fmt.Println("file open failed", err )
		return
	}
	err = binary.Read( f, binary.BigEndian, &oftb.ofascaler )
	err = binary.Read( f, binary.BigEndian, &oftb.numTables )
	err = binary.Read( f, binary.BigEndian, &oftb.searchRange )
	err = binary.Read( f, binary.BigEndian, &oftb.entrySelector )
	err = binary.Read( f, binary.BigEndian, &oftb.rangeShift )

	td := make( map[string]table_directory_entry )

	var i uint16
	for i = 0; i< oftb.numTables; i++ {
		tmp := table_directory_entry{}
		tag := [4]uint8{}
		err = binary.Read( f, binary.BigEndian, &tag )
		buf := new(bytes.Buffer)
		err = binary.Write(buf, binary.BigEndian, tag)
		err = binary.Read( f, binary.BigEndian, &tmp.checkSum )
		err = binary.Read( f, binary.BigEndian, &tmp.offset )
		err = binary.Read( f, binary.BigEndian, &tmp.length )
		td[buf.String()] = tmp
	}
	if err != nil {
		fmt.Println("binary read failed", err )
		return
	}
	fmt.Println("table checkSum offset   length");
	for key,value := range td {
		fmt.Printf( "%s %08x\n", key, value )
	}

}


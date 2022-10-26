package main

import (
  "os"
  "math"
  "fmt"
  "os/exec"
  "github.com/iceber/iouring-go"
  "github.com/creack/pty"
  "golang.org/x/term"
)

func main() {
  const num = 8
  ring, err := iouring.New(num, iouring.WithAsync())

  results := make(chan iouring.Result, num)

  buffers := [num][4096]byte{}
  requests := [num]iouring.PrepRequest{}

  command := os.Args[1:]
  c := exec.Command(command[0], command[1:]...)

  f, err := pty.Start(c)
  term.MakeRaw(int(f.Fd()))

  if err != nil {
    panic("Failed to start the command")
  }

  for i := 0; i < num; i++ {
    requests[i] = iouring.Read(int(f.Fd()), buffers[i][:]).WithInfo(i)
  }
  ring.SubmitRequests(requests[:], results)

  prev := 0
  n := 0
  k := 0
  for i := range results {
    // print("Here")
    b0, _  := i.GetRequestBuffer()
    res, err := i.ReturnInt()
    z := i.GetRequestInfo().(int)
    fmt.Printf("Request Info: %d\n", z)
    // math.Abs(float64(res - prev))
    if math.Abs(float64(res - prev)) > 10 {
      // fmt.Printf("New Result: %d\n", res)
      prev = res
      n++
    }
    if err != nil  || res == -1 {
      if err != nil { fmt.Println(err) }
      ring.Close()
      c.Wait()
      break
    }
    ring.SubmitRequest(iouring.Read(int(f.Fd()), b0).WithInfo(z), results)
    k++
    // fmt.Printf("%s", b0)
  }
  fmt.Printf("n: %d, k: %d", n, k)
  // ring.Close()
}


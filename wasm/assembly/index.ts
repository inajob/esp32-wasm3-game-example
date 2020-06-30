import * as dev from "./arduino";

let y = 0;
let t = 0;

function run(): void {
  dev.println('Hello1!');
  for(let y = 0; y < 128; y ++){
    for(let x = 0; x < 128; x ++){
      dev.color((x)%255, (t*t*x)%255, (t*y)%255);
      dev.pset(x, y);
    }
  }
  t ++;
}

export function _start(): void{
    run();
}


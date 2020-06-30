export declare function millis(): u32;
export declare function delay(ms: u32): void;
export declare function pset(x: u32, y: u32): void;
export declare function color(r: u32, g: u32, b: u32): void;

@external("print")
declare function _print(ptr: usize, len: usize): void;

export function print(str: string): void {
  const buffer = String.UTF8.encode(str);
  _print(changetype<usize>(buffer), buffer.byteLength);
}

export function println(str: string): void {
  print(str);
  print('\n');
}

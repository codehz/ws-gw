import Client from "https://deno.hertz.services/codehz/ws-gateway/sdk/client.ts";

const client = new Client();

console.log(1);
await client.connect("ws://127.0.0.1:8808/", console.error);
console.log(2);
const srv = await client.get("test");
console.log(3);
await srv.waitOnline();
console.log(4);
console.log(await srv.call("tip", new Uint8Array));
console.log(5);
window.close();
# Proxy COM identity and lifetime

The proxy returns its own `IDirect3D9` and `IDirect3DDevice9` objects for `IUnknown` and their base-interface queries.
Each wrapped device retains its outer D3D9 owner, and `GetDirect3D` returns that owner with an additional reference.
Wrapper reference counts use interlocked operations.

When the chain-loaded module exports `BridgeClient_SetOuterDevice`, the proxy registers the inner device, outer
device, and outer D3D9 owner after construction. The bridge then returns these outer objects from resource
`GetDevice` and device `GetDirect3D` calls. An older chain without this export remains supported.

The Remix Plus 2.0 registration contract borrows these pointers without retaining them. The proxy clears registration
before its final explicit device reference releases the inner device or frees either wrapper. This prevents a bridge
resource from returning a dangling outer pointer.

If resources outlive every explicit reference to the outer device, the inner device may remain alive. After
unregistration, a resource's `GetDevice` returns that live inner device, which bypasses this proxy's draw interception.
Full outer identity during that resource-only lifetime would require resource wrappers or a bridge destruction
callback. Keeping an untracked outer wrapper alive would leak it, and adding permanent reciprocal references would
create a cycle. The current integration follows the borrowed-pointer contract and requires the application to retain
its outer device while it needs proxy interception.

The isolated `test-artifacts/JPOG/plus2-migration/proxy-com-test` fixture loads the actual built proxy with synthetic
COM chain modules. It checks both export-present and export-absent cases, interface identity, parent ownership,
resource `GetDevice` behavior, registration clearing before inner destruction, and resource-only lifetime without
retaining dangling outer pointers. These checks validate the proxy against the bridge's documented ownership model;
they do not replace a live game check with the actual bridge.

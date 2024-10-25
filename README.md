# esphome-components
You can use these components as [External Component](https://esphome.io/components/external_components)

## fisher_ir
This climate component allows you to control a Fisher AC units by sending an infrared (IR) control signal, just as the unit’s handheld (NT-10A) remote controller would.
Receiver is supported, you can optionally add a Remote Receiver component so the climate state will be tracked when it is operated with the original remote controller unit.
This component is based on the [emmeti](https://github.com/esphome/esphome/pull/5197) climate component 

Example usage:
https://github.com/p5ych08illy/esphome-components/blob/72d06b73061753cc70ac1e7ff3d4d7dea4bd9164/example_fisher.yaml#L3-L20

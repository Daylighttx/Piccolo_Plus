"""Dump the action tree and selected Toon Outline draw state from a capture.

Run with ``qrenderdoc.exe --ui-python analyze_renderdoc_capture.py capture.rdc``.
The report path can be overridden with ``PICCOLO_RENDERDOC_REPORT``.
"""

import os
import traceback


REPORT_PATH = os.environ.get("PICCOLO_RENDERDOC_REPORT") or "capture-report.txt"

with open(REPORT_PATH, "w", encoding="utf-8") as stream:
    stream.write("RenderDoc analysis script started\n")


def write_report(text: str) -> None:
    with open(REPORT_PATH, "a", encoding="utf-8") as stream:
        stream.write(text + "\n")


def value(obj, name, fallback="<not available>"):
    try:
        return getattr(obj, name)
    except Exception:
        return fallback


def enum_name(enum_type, raw_value):
    try:
        return enum_type(int(raw_value)).name
    except Exception:
        return str(raw_value)


def buffer_size_text(size):
    if size in (0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF):
        return "whole buffer"
    return f"{size} bytes"


def write_draw_state(controller, action, rd):
    write_report("")
    write_report("=== Toon Outline draw state ===")
    write_report(f"Event ID: {action.eventId}; Action ID: {action.actionId}")
    write_report(
        f"Draw: {action.GetName(controller.GetStructuredFile())}; "
        f"indices/vertices={action.numIndices}; instances={action.numInstances}; "
        f"firstIndex={action.indexOffset}; baseVertex={action.baseVertex}; "
        f"instanceOffset={action.instanceOffset}"
    )

    controller.SetFrameEvent(action.eventId, True)
    pipe = controller.GetPipelineState()
    write_report(f"Graphics pipeline resource: {pipe.GetGraphicsPipelineObject()}")
    write_report(f"Topology: {enum_name(rd.Topology, pipe.GetPrimitiveTopology())}")

    raster = pipe.GetRasterState()
    depth = pipe.GetDepthTestState()
    write_report(
        f"Raster: fill={enum_name(rd.FillMode, raster.fillMode)}; "
        f"cull={enum_name(rd.CullMode, raster.cullMode)}; "
        f"frontCCW={raster.frontCCW}"
    )
    write_report(
        f"Depth: test={depth.depthEnable}; write={depth.depthWrites}; "
        f"compare={enum_name(rd.CompareFunction, depth.depthFunction)}"
    )

    index_buffer = pipe.GetIBuffer()
    write_report(
        f"Index buffer: resource={index_buffer.resourceId}; "
        f"offset={index_buffer.byteOffset}; stride={index_buffer.byteStride}; "
        f"size={buffer_size_text(index_buffer.byteSize)}"
    )
    for slot, buffer in enumerate(pipe.GetVBuffers()):
        write_report(
            f"Vertex buffer[{slot}]: resource={buffer.resourceId}; "
            f"stride={buffer.byteStride}; offset={buffer.byteOffset}; "
            f"size={buffer_size_text(buffer.byteSize)}"
        )
    for attribute in pipe.GetVertexInputs():
        write_report(
            f"Vertex input: name={attribute.name}; used={attribute.used}; "
            f"slot={attribute.vertexBuffer}; "
            f"offset={attribute.byteOffset}; perInstance={attribute.perInstance}; "
            f"format={attribute.format.Name()}"
        )

    for used in pipe.GetAllUsedDescriptors(False):
        access = used.access
        descriptor = used.descriptor
        write_report(
            f"Descriptor access: stage={access.stage}; reflectedIndex={access.index}; "
            f"type={access.type}; store={access.descriptorStore}; "
            f"accessOffset={access.byteOffset}; accessSize={access.byteSize}; "
            f"staticallyUnused={access.staticallyUnused}; "
            f"resource={descriptor.resource}; descriptorOffset={descriptor.byteOffset}; "
            f"descriptorSize={descriptor.byteSize}"
        )

    vk = controller.GetVulkanPipelineState()
    if vk is not None:
        write_report(
            f"Vulkan pass: subpass={value(value(vk.currentPass, 'renderpass'), 'subpass')}; "
            f"cullMode={enum_name(rd.CullMode, value(vk.rasterizer, 'cullMode'))}; "
            f"depthTest={value(vk.depthStencil, 'depthTestEnable')}; "
            f"depthWrite={value(vk.depthStencil, 'depthWriteEnable')}; "
            f"depthCompare={enum_name(rd.CompareFunction, value(vk.depthStencil, 'depthFunction'))}"
        )

try:
    import renderdoc as rd

    def walk_actions(controller, actions, depth=0, in_outline=False, outline_draws=None):
        structured = controller.GetStructuredFile()
        for action in actions:
            name = action.GetName(structured)
            write_report(f"{'  ' * depth}[{action.eventId}] {name}")
            current_in_outline = in_outline or name == "Toon Outline"
            if current_in_outline and "vkCmdDrawIndexed" in name:
                outline_draws.append(action)
            if action.children:
                walk_actions(controller,
                             action.children,
                             depth + 1,
                             current_in_outline,
                             outline_draws)

    def analyze(controller):
        api_name = "Vulkan" if controller.GetVulkanPipelineState() is not None else controller.GetAPIProperties().pipelineType
        write_report(f"API: {api_name}")
        outline_draws = []
        walk_actions(controller, controller.GetRootActions(), outline_draws=outline_draws)
        if outline_draws:
            write_draw_state(controller, outline_draws[0], rd)
        else:
            write_report("No indexed draw was found under the Toon Outline marker.")

    pyrenderdoc.Replay().BlockInvoke(analyze)
    write_report("RenderDoc analysis script finished")
except Exception:
    write_report(traceback.format_exc())
    raise

package org.erpiopio.legacygoku2.thor;

/** Calls the production C++ JNI through a real JVM with -Xcheck:jni. */
final class Legacy2MenuJniTest {
    private static native void publish(int generation, int selected, boolean available);
    private static native Object[] read(int selected);
    private static native void publishCopiedState(byte[] rom, byte[] memory);

    private static void verifyCopiedState(String romPath, String statePath) throws Exception {
        byte[] rom = java.nio.file.Files.readAllBytes(java.nio.file.Path.of(romPath));
        byte[] state = java.nio.file.Files.readAllBytes(java.nio.file.Path.of(statePath));
        java.nio.ByteBuffer bytes = java.nio.ByteBuffer.wrap(state)
            .order(java.nio.ByteOrder.LITTLE_ENDIAN);
        byte[] memory = null;
        int count = bytes.getInt(48), offset = 52;
        for (int n = 0; n < count; ++n) {
            int size = bytes.getInt(offset + 4);
            if (state[offset] == 'B' && state[offset+1] == 'U' &&
                    state[offset+2] == 'S' && state[offset+3] == '0')
                memory = java.util.Arrays.copyOfRange(state, offset + 8, offset + 8 + size);
            offset += 8 + size;
        }
        check(memory != null, "GBAS BUS0 memory exists");
        publishCopiedState(rom, memory);
        Legacy2MenuSnapshot copy = Legacy2MenuSnapshot.decode(read(-1));
        check(copy != null && copy.state.activeId == 3 && copy.selectedId == 3,
            "copied snapshot selects active Trunks");
        Legacy2EstadoModel.CharacterState trunks = copy.state.characters[3];
        check(copy.state.availableIds.length == 1 && trunks.level == 6 &&
            trunks.hp == 149 && trunks.hpMax == 149 && trunks.energy == 39 &&
            trunks.energyMax == 39 && trunks.experience == 1000 &&
            trunks.nextExperience == 550, "copied snapshot known Trunks statistics");
        check(copy.portrait == null, "no fabricated portrait without decoder bus");
        check(java.util.Arrays.equals(copy.skillSelection, new int[]{3, -1, 0, 5000}),
            "copied base Ki selection does not highlight a special or enable input without bus");
        check(java.util.Arrays.equals(copy.skillIconIds, new int[]{
                -1 - (memory[0x40000 + 0x10B8 + 3 * 0x1C + 8] & 255), 1, 13}),
            "copied Trunks technique types come from the native record");
        check(copy.items[1].equals("Alubia Senzu") &&
            copy.items[37*4+1].equals("Libro de Matem\u00e1ticas") &&
            copy.items[45*4+1].equals("Radar Drag\u00f3n"), "authored Spanish item IDs");
        for (int id = 0; id < 46; ++id)
            check(copy.items[id*4].equals(Integer.toString(id)) &&
                !copy.items[id*4+1].isEmpty() && !copy.items[id*4+2].isEmpty() &&
                Integer.parseInt(copy.items[id*4+3]) >= 0, "all copied item fields");
        for (int id = 0; id < 43; ++id)
            check(copy.journal[id*4].equals(Integer.toString(id)) &&
                !copy.journal[id*4+1].isEmpty() &&
                Integer.parseInt(copy.journal[id*4+2]) >= 0 &&
                Integer.parseInt(copy.journal[id*4+2]) <= 2, "all copied quest fields");
        System.out.println("PASS: production decoders through JNI/JVM over real copied GBAS memory: Trunks L6 149/149 39/39 EXP1000 SIG550, 46 item IDs and 43 missions");
    }

    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static void verify(Object[] raw) {
        check(raw != null && raw.length == 8, "eight-part JNI publication");
        Legacy2MenuSnapshot packet = Legacy2MenuSnapshot.decode(raw);
        check(packet != null, "complete JNI packet");
        int generation = packet.state.generation;
        String text = Integer.toString(generation);
        check(packet.selectedId == 3, "native fallback to available active character");
        check(packet.state.characters[3].hp == generation, "status publication");
        check(packet.portrait.length == 3 && packet.portrait[2] == generation, "portrait generation");
        check(java.util.Arrays.equals(packet.skillSelection,
                new int[]{3, generation % 2 + 1, 0, generation}),
            "confirmed technique selection shares the status generation");
        check(java.util.Arrays.equals(packet.skillIconIds,
                new int[]{-1, generation % 14, (generation + 1) % 14}),
            "technique icon types share the status generation");
        for (int n = 0; n < 3; ++n)
            check(packet.skills[n * 3].equals(text) &&
                packet.skills[n * 3 + 1].equals(text), "technique generation");
        for (int n = 0; n < 46; ++n)
            check(packet.items[n * 4].equals(Integer.toString(n)) &&
                packet.items[n * 4 + 1].equals(text) &&
                packet.items[n * 4 + 2].equals(text) &&
                packet.items[n * 4 + 3].equals(text), "item publication");
        for (int n = 0; n < 43; ++n)
            check(packet.journal[n * 4].equals(Integer.toString(n)) &&
                packet.journal[n * 4 + 1].equals(text) &&
                packet.journal[n * 4 + 2].equals(Integer.toString(generation % 3)),
                "quest publication");
    }

    public static void main(String[] args) throws Exception {
        System.load(args[0]);
        check(read(-1) == null, "no fabricated pre-publication state");
        publish(1, 3, true);
        verify(read(-1));
        verify(read(5)); // locked selection is never exposed

        Object[] invalid = read(3);
        invalid[1] = new int[]{5};
        check(Legacy2MenuSnapshot.decode(invalid) == null, "reject locked selection");
        invalid = read(3);
        invalid[4] = new String[46 * 4 - 1];
        check(Legacy2MenuSnapshot.decode(invalid) == null, "reject truncated inventory");

        Thread publisher = new Thread(() -> {
            for (int generation = 2; generation <= 4000; ++generation)
                publish(generation, 3, true);
        });
        publisher.start();
        for (int n = 0; n < 3000; ++n) verify(read(3));
        publisher.join();
        publish(4001, 3, false);
        Legacy2MenuSnapshot empty = Legacy2MenuSnapshot.decode(read(3));
        check(empty != null && empty.selectedId == -1 &&
            empty.state.availableIds.length == 0 && empty.portrait == null &&
            empty.skills == null &&
            java.util.Arrays.equals(empty.skillSelection, new int[]{-1, -1, 0, 4001}) &&
            java.util.Arrays.equals(empty.skillIconIds, new int[]{-1, -1, -1}),
            "state replacement clears locked character and technique selection");

        Legacy2EstadoModel model = new Legacy2EstadoModel();
        model.update(empty.state);
        model.clear();
        check(model.selected() == null && model.count() == 0, "explicit invalidation clears state");

        for (int[] dimensions : new int[][]{
                {1240,1080},{620,540},{1920,1080},{1080,1240}}) {
            Legacy2MenuViewport view = new Legacy2MenuViewport(dimensions[0],dimensions[1]);
            float screenX = view.left + 1156 * view.scale;
            float screenY = view.top + 86 * view.scale;
            check(Math.abs(view.x(screenX) - 1156) < 0.001 &&
                Math.abs(view.y(screenY) - 86) < 0.001 &&
                view.contains(screenX,screenY), "drawing and touch transforms agree");
            check(!view.contains(view.left - 1,view.top - 1), "ignore letterbox touches");
        }
        Legacy2MenuViewport thor = new Legacy2MenuViewport(1240,1080);
        check(thor.scale == 1 && thor.left == 0 && thor.top == 0, "Thor preserves original pixel layout");
        System.out.println("PASS: production JNI, 3000 concurrent atomic snapshots, 46 items, 43 quests, selection invalidation, viewport");
        if (args.length == 3) verifyCopiedState(args[1],args[2]);
    }
}

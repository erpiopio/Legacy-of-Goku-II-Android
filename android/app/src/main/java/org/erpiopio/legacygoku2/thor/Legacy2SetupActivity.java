package org.erpiopio.legacygoku2.thor;

import org.gbarecomp.GbaSetupActivity;
import java.io.File;
import java.io.IOException;

public final class Legacy2SetupActivity extends GbaSetupActivity {
    @Override
    protected Class<?> gameActivityClass() { return Legacy2GameActivity.class; }
    @Override
    protected void prepareRomResources(File rom) throws IOException {
        try { Legacy2ScouterResources.prepare(this,rom); }
        catch(Exception error) { throw new IOException("Scouter: "+error.getMessage(),error); }
        try { Legacy2ItemResources.prepare(this,rom); }
        catch(Exception error) { throw new IOException("Objetos: "+error.getMessage(),error); }
        try { Legacy2SkillResources.prepare(this,rom); }
        catch(Exception error) { throw new IOException("Habilidades: "+error.getMessage(),error); }
    }
}

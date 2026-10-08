import asyncio, os, time
from playwright.async_api import async_playwright
EXE = '/opt/pw-browsers/chromium' if os.path.exists('/opt/pw-browsers/chromium') else None
URL = 'http://127.0.0.1:8765/'
V = "document.getElementById('v')"


async def pick(page, video):
    await page.evaluate("document.getElementById('msg').textContent='';document.getElementById('msg').className=''")
    await page.set_input_files('#file', video)
    await page.wait_for_function("document.getElementById('msg').textContent.indexOf('second video')>0", timeout=20000)


async def slide(page, val):
    await page.evaluate(f"()=>{{const s=document.getElementById('start');s.value={val};s.dispatchEvent(new Event('input'))}}")


async def convert(page, label):
    t = time.time()
    await page.click('#go')
    await page.wait_for_function("document.getElementById('go').disabled", timeout=5000)
    await page.wait_for_function("document.getElementById('msg').className!=''", timeout=180000)
    msg = await page.inner_text('#msg')
    cls = await page.get_attribute('#msg', 'class')
    print(f'  {label:44s} -> [{cls}] {msg}  ({time.time()-t:.1f}s)')
    return cls == 'ok'


async def one(page, video, start, length, fps, fit, name, label):
    await pick(page, video)
    await slide(page, start)
    await page.select_option('#len', str(length))
    await page.select_option('#fps', str(fps))
    await page.select_option('#fit', fit)
    if name is not None:
        await page.fill('#name', name)
    return await convert(page, label)


async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch(executable_path=EXE)
        ctx = await b.new_context(viewport={'width': 390, 'height': 844}, device_scale_factor=2)
        page = await ctx.new_page()
        errs = []
        page.on('pageerror', lambda e: errs.append(str(e)))
        page.on('console', lambda m: errs.append(m.text) if m.type == 'error' and '404' not in m.text else None)
        await page.goto(URL)
        await page.wait_for_timeout(400)
        res = {}
        # name box is empty with a placeholder
        await pick(page, 'land.webm')
        res['name box starts empty'] = (await page.input_value('#name')) == ''
        res['placeholder shows the file name'] = (await page.get_attribute('#name', 'placeholder')) == 'land'
        # live preview while dragging
        shots = []
        for val in (1.0, 4.0, 7.5, 9.2):
            await slide(page, val)
            await page.wait_for_timeout(350)
            ct = await page.evaluate(V + ".currentTime")
            shots.append((val, ct, await (await page.query_selector('.tv')).screenshot()))
        res['preview follows the slider'] = all(abs(v - c) < 0.06 for v, c, _ in shots)
        res['preview picture changes as you drag'] = len({s for _, _, s in shots}) == 4
        open('scrub.png', 'wb').write(shots[2][2])
        for val in (0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.0):
            await slide(page, val)
        await page.wait_for_timeout(700)
        ct = await page.evaluate(V + ".currentTime")
        res['fast drag lands on the last spot'] = abs(ct - 6.0) < 0.06
        res['label shows start and end'] = (await page.inner_text('#st'), await page.inner_text('#en')) == ('6.0', '11.0')
        # play preview button
        await slide(page, 2.0)
        await page.select_option('#len', '3')
        await page.wait_for_timeout(300)
        await page.click('#pv')
        await page.wait_for_timeout(600)
        mid = await page.evaluate(f"[{V}.paused,{V}.currentTime,document.getElementById('pv').textContent]")
        await page.wait_for_function("document.getElementById('pv').textContent=='Play preview'", timeout=8000)
        await page.wait_for_timeout(300)
        end = await page.evaluate(f"[{V}.paused,{V}.currentTime]")
        res['Play preview plays the chosen part'] = (mid[0] is False and 2.0 < mid[1] < 4.0 and mid[2] == 'Stop preview')
        res['preview stops and rewinds to the start'] = (end[0] is True and abs(end[1] - 2.0) < 0.1)
        await page.screenshot(path='page2.png', full_page=True)
        # conversions, fast path
        ok = []
        ok.append(await one(page, 'land.webm', 2.0, 5, 12, 'fill', 'My Clip!! one', 'land 5s @12fps fill, typed name'))
        ok.append(await one(page, 'port.webm', 1.0, 3, 15, 'fit', None, 'port 3s @15fps fit, name left blank'))
        ok.append(await one(page, 'port.webm', 0.0, 3, 10, 'fill', 'portfill', 'port 3s @10fps fill'))
        ok.append(await one(page, 'land.webm', 10.5, 5, 15, 'fill', 'tail', 'land last 1.5s @15fps (asked for 5s)'))
        ok.append(await one(page, 'land.webm', 0.0, 10, 15, 'fill', 'ten', 'land 10s @15fps'))
        await page.evaluate("MAXF=5000")
        ok.append(await one(page, 'land.webm', 3.0, 3, 12, 'fill', 'squeezed', 'oversized frames squeezed (limit 5 KB)'))
        await page.evaluate("MAXF=26000")
        res['all fast-path conversions finished'] = all(ok)
        await page.wait_for_timeout(1800)
        res['list'] = (await page.inner_text('#list')).replace('\n', ' | ')
        # old-browser path: no requestVideoFrameCallback
        ctx2 = await b.new_context(viewport={'width': 390, 'height': 844})
        await ctx2.add_init_script("delete HTMLVideoElement.prototype.requestVideoFrameCallback")
        p2 = await ctx2.new_page()
        p2.on('pageerror', lambda e: errs.append('p2 ' + str(e)))
        await p2.goto(URL)
        await p2.wait_for_timeout(300)
        res['careful path (older browser) finished'] = await one(p2, 'land.webm', 2.0, 5, 12, 'fill', 'oldway', 'land 5s @12fps, careful one-by-one path')
        for k, vv in res.items():
            print(('PASS' if vv is True else ('INFO' if not isinstance(vv, bool) else 'FAIL')), k, '' if isinstance(vv, bool) else vv)
        print('page errors:', errs or 'none')
        await b.close()

asyncio.run(main())

CREATE SEQUENCE IF NOT EXISTS orders_order_id_seq START 1;

CREATE TABLE IF NOT EXISTS seats (
    event_id TEXT NOT NULL,
    seat_id TEXT NOT NULL,
    price_cents INTEGER NOT NULL CHECK (price_cents >= 0),
    status TEXT NOT NULL CHECK (status IN ('AVAILABLE', 'HELD', 'SOLD', 'REFUNDED')),
    last_order_id TEXT,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (event_id, seat_id)
);

CREATE TABLE IF NOT EXISTS orders (
    order_id TEXT PRIMARY KEY DEFAULT ('ORD-' || nextval('orders_order_id_seq')),
    user_id TEXT NOT NULL,
    event_id TEXT NOT NULL,
    seat_id TEXT NOT NULL,
    price_cents INTEGER NOT NULL CHECK (price_cents >= 0),
    hold_token BYTEA NOT NULL UNIQUE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS refunds (
    refund_id BIGSERIAL PRIMARY KEY,
    user_id TEXT NOT NULL,
    order_id TEXT NOT NULL REFERENCES orders(order_id) ON DELETE CASCADE,
    amount_cents INTEGER NOT NULL CHECK (amount_cents >= 0),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);